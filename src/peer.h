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
        }

        void addPeer(uint32_t nodeId, int channel=-1) {
          for(int i=0; i<PEER_MAX; i++) {
            if (peers[i].nodeId == nodeId) {
              peers[i].channel = channel;
              return;
            }
          }
          for(int i=0; i<PEER_MAX; i++) {
            if (peers[i].nodeId == 0) {
              peers[i].nodeId = nodeId;
              peers[i].channel = channel;
              return;
            }
          }
        }

        void removePeer(uint32_t nodeId) {
          for(int i=0; i<PEER_MAX; i++) {
            if (peers[i].nodeId == nodeId) {
              peers[i].nodeId = 0;
              peers[i].channel = -1;
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

        uint32_t* nodeList() {
          uint32_t list[PEER_MAX];
          for(int i=0; i<PEER_MAX; i++) {
            list[i] = peers[i].nodeId;
          }
          return list;
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
              if (!found) {
                peers[i].nodeId = 0;
                peers[i].channel = -1;
              }
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

        int size() {
          int count = 0;
          for(int i=0; i<PEER_MAX; i++) {
            if (peers[i].nodeId != 0) count++;
          }
          return count;
        }

        int position() {
          int pos = 0;
          for(int i=0; i<PEER_MAX; i++)
            if (peers[i].channel >= 0 && peers[i].channel < _channel) pos++;
          return pos;
        }

        bool isSolo() {
          return size() == 0;
        }

        bool isMaster() {
          return !isSolo() && position() == 0;
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
       
};

#endif