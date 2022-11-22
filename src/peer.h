#ifndef K32_peer_h
#define K32_peer_h

#include <Arduino.h>
#include <list>

#define PEER_MAX 16

struct Peer { // This structure is named "myDataType"
  uint32_t nodeId;
  int channel;
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

            if (nodeId.toInt() == _nodeId) _channel = channel.toInt();
            else addPeer(nodeId.toInt(), channel.toInt());
          }
        }

        void clear() 
        {
          for(int i=0; i<PEER_MAX; i++) {
            peers[i].nodeId = 0;
            peers[i].channel = -1;
          }
          _dirty = true;
        }

        void addPeer(uint32_t nodeId, int channel=-1) {
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
              if (!found) removePeer(peers[i].nodeId);
            }
          }

          // add missing nodes
          node = nodes.begin();
          while (node != nodes.end()) {
            bool found = false;
            for(int i=0; i<PEER_MAX; i++) {
              if (peers[i].nodeId == *node) {
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
          clear();
          addPeer(pool->ownerID(), pool->ownerChannel()); // add pool owner as peer

          for(int i=0; i<PEER_MAX; i++) {
            if (pool->peers[i].nodeId != 0 && pool->peers[i].nodeId != _nodeId) {
              addPeer(pool->peers[i].nodeId, pool->peers[i].channel);
            }
          }
        }

        int size() 
        {
          calculate();
          return _size;
        }

        int count() 
        {
          calculate();
          return _distinctChannels;
        }

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
          for(int i=0; i<_channel; i++) {
            for(int j=0; j<PEER_MAX; j++)
              if (peers[j].nodeId != 0 && peers[j].channel >= 0 && peers[j].channel < i) {
                _chanPosition++;
                break;
              }
          }

          // Peers position
          _peerPosition = 0;
          for(int i=0; i<PEER_MAX; i++)
            if (peers[i].channel >= 0) 
              if (peers[i].channel < _channel) _peerPosition++;
              else if (peers[i].channel == _channel && peers[i].nodeId < _nodeId) _peerPosition++;

          _dirty = false;
        }

        bool isSolo() {
          return size() == 0;
        }

        bool isMaster() {
          return !isSolo() && _peerPosition == 0;
        }

        uint32_t ownerID() {
          return _nodeId;
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
        int _chanPosition = 0;
        int _peerPosition = 0;
        int _distinctChannels = 0;

        bool _dirty = true;
       
};

#endif