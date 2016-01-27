#ifndef PACKETHANDLER_H
#define PACKETHANDLER_H

#include <memory>

#include "PacketHandlerOptions.h"
#include "Timer.h"

namespace distdpi {

class PacketHandler: public Timer {
    public:

        PacketHandler(PacketHandlerOptions options);
        ~PacketHandler();
        void start();
        void PacketProducer(uint8_t *pkt, uint32_t len);
    private:
        std::shared_ptr<PacketHandlerOptions> options_;
    protected:
        virtual void executeCb();
};


}
#endif