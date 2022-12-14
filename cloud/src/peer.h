#ifndef K32_peer_h
#define K32_peer_h

#include <Arduino.h>
#include <list>

#define PEER_MAX 16

struct Peer { // This structure is named "myDataType"
  uint32_t nodeId;
  int channel;
  int missing;
};


class PeersPool  {
    public:
        PeersPool(uint32_t nodeId, int channel) 
        {
          _nodeId = nodeId;
          _channel = channel;

          clear();
        }

        PeersPool(String peerlist, uint32_t masterId) 
        {
          _nodeId = masterId;

          clear();

          // Parse peerlist
          //
          int pos = 0;
          while (peerlist.length() > 0) {
            int pos = peerlist.indexOf(",");
            if (pos == -1) pos = peerlist.length();
            String peer = peerlist.substring(0, pos);
            peerlist = peerlist.substring(pos+1);

            int pos2 = peer.indexOf("=");
            if (pos2 == -1) continue;
            String nodeId = peer.substring(0, pos2);
            String channel = peer.substring(pos2+1);
            
            unsigned long nID = strtoul(nodeId.c_str(), NULL, 10); ;

            if (nID == _nodeId) _channel = channel.toInt();
            else addPeer(nID, channel.toInt());
          }
        }

        void clear() 
        {
          for(int i=0; i<PEER_MAX; i++) {
            peers[i].nodeId = 0;
            peers[i].channel = -1;
            peers[i].missing = 0;
          }
          _dirty = true;
        }

        void addPeer(uint32_t nodeId, int channel=-1) {
          // LOG("Add peer: "+String(nodeId)+"="+String(channel));
          for(int i=0; i<PEER_MAX; i++) {
            if (peers[i].nodeId == nodeId) {
              peers[i].channel = channel;
              _dirty = true;
              return;
            }
          }
          for(int i=0; i<PEER_MAX; i++) {
            if (peers[i].nodeId == 0) {
              peers[i].nodeId = nodeId;
              peers[i].channel = channel;
              peers[i].missing = 0;
              _dirty = true;
              return;
            }
          }
        }

        void removePeer(uint32_t nodeId) {
          for(int i=0; i<PEER_MAX; i++) {
            if (peers[i].nodeId == nodeId) {
              peers[i].nodeId = 0;
              peers[i].channel = -1;
              _dirty = true;
              return;
            }
          }
        }

        void missingPeer(uint32_t nodeId) {
          for(int i=0; i<PEER_MAX; i++) {
            if (peers[i].nodeId == nodeId) {
              peers[i].missing = true;
              _dirty = true;
              return;
            }
          }
        }

        int getChannel(uint32_t nodeId) {
          for(int i=0; i<PEER_MAX; i++) {
            if (peers[i].nodeId == nodeId) {
              return peers[i].channel;
            }
          }
          return -1;
        }


        void updatePeers(std::list<uint32_t> nodes) 
        {
          std::list<uint32_t>::iterator node = nodes.begin();

          // remove peers not in the list
          for(int i=0; i<PEER_MAX; i++) {
            if (peers[i].nodeId != 0) {
              bool found = false;
              node = nodes.begin();
              while (node != nodes.end()) {
                if (peers[i].nodeId == *node) {
                  found = true;
                  break;
                }
                node++;
              }
              if (!found) missingPeer(peers[i].nodeId);
            }
          }

          // add missing nodes
          node = nodes.begin();
          while (node != nodes.end()) {
            bool found = false;
            for(int i=0; i<PEER_MAX; i++) {
              if (peers[i].nodeId == *node) {
                peers[i].missing = 0;
                found = true;
                break;
              }
            }
            if (!found) addPeer(*node);
            node++;
          }
        }

        void import(PeersPool* pool) 
        {
          // clear();
          addPeer(pool->ownerID(), pool->ownerChannel()); // add pool owner as peer

          for(int i=0; i<PEER_MAX; i++) {
            if (pool->peers[i].nodeId != 0 && pool->peers[i].nodeId != _nodeId) {
              addPeer(pool->peers[i].nodeId, pool->peers[i].channel);
            }
          }
        }

        // Number of other peers with valid channels
        int size() 
        {
          calculate();
          return _size;
        }

        // Number of other peers with valid channels && not missing
        int sizeLocal() 
        {
          calculate();
          return _sizeLocal;
        }

        // Number of distinct active channels
        int count() 
        {
          calculate();
          return _distinctChannels;
        }

        // Position of owner in the active channel list
        int position() 
        {
          calculate();
          return _chanPosition;
        }

        void calculate() 
        {
          if (!_dirty) return;

          // Size
          _size = 0;
          for(int i=0; i<PEER_MAX; i++)
            if (peers[i].nodeId != 0 && peers[i].channel > -1) _size++;

          // LOCAL Size
          _sizeLocal = 0;
          for(int i=0; i<PEER_MAX; i++)
            if (peers[i].nodeId != 0 && peers[i].channel > -1 && peers[i].missing == 0) _sizeLocal++;
        
          // Distinct channels
          int channels[32] = {0};
          if (_channel > -1) channels[_channel] = 1;

          for(int i=0; i<PEER_MAX; i++)
            if (peers[i].nodeId != 0 && peers[i].channel > -1)
              channels[peers[i].channel]++;

          _distinctChannels = 0;
          for(int i=0; i<32; i++)
            if (channels[i] > 0) _distinctChannels++;

          // Chan Position
          _chanPosition = 0;
          for(int i=0; i<_channel; i++)
            if (channels[i] > 0) _chanPosition++;

          // Peers position
          _peerPosition = 0;
          for(int i=0; i<PEER_MAX; i++)
            if (peers[i].channel >= 0) 
              if (peers[i].channel < _channel) _peerPosition++;
              else if (peers[i].channel == _channel && peers[i].nodeId < _nodeId) _peerPosition++;

          // Peers LOCAL position
          _peerLocalPosition = 0;
          for(int i=0; i<PEER_MAX; i++)
            if (peers[i].channel >= 0 && peers[i].missing == 0) 
              if (peers[i].channel < _channel) _peerLocalPosition++;
              else if (peers[i].channel == _channel && peers[i].nodeId < _nodeId) _peerLocalPosition++;


          _dirty = false;
        }

        bool isSolo() {
          return sizeLocal() == 0;
        }

        bool isMaster() {
          return !isSolo() && _peerPosition == 0;
        }

        bool isLocalMaster() {
          return !isSolo() && _peerLocalPosition == 0;
        }

        uint32_t ownerID() {
          return _nodeId;
        }

        void ownerID(uint32_t id) {
          _nodeId = id;
        }

        uint32_t masterID() {
          if (isSolo()) return 0;
          uint32_t mID = 0;
          int mchan = 0;
          for(int i=0; i<PEER_MAX; i++)
            if (peers[i].channel >= 0 && peers[i].missing == 0) 
              if (mID == 0 || peers[i].channel < mchan || (peers[i].channel == mchan && peers[i].nodeId < mID) ) {
                mID = peers[i].nodeId;
                mchan = peers[i].channel;
              }
          return mID;
        }

        int ownerChannel() {
          return _channel;
        }

        String toString() {
          String str = String(_nodeId) + "=" + String(_channel);
          for(int i=0; i<PEER_MAX; i++)
            if (peers[i].nodeId != 0 && peers[i].channel != -1)
              str += "," + String(peers[i].nodeId) + "=" + String(peers[i].channel);

          return str;
        }
    
        Peer peers[PEER_MAX];

      private:
        uint32_t _nodeId = 0;
        int _channel = -1;

        int _size = 0;
        int _sizeLocal = 0;
        int _chanPosition = 0;
        int _peerPosition = 0;
        int _peerLocalPosition = 0;
        int _distinctChannels = 0;

        bool _dirty = true;
       
};

#endif