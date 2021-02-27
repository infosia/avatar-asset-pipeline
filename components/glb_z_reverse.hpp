#pragma once

#include <iostream>
#include <DSPatch.h>

namespace DSPatch {

class glb_z_reverse final : public Component {

public:
    glb_z_reverse()
    {

    }

    virtual ~glb_z_reverse()
    {    
    
    }

protected:
    virtual void Process_(SignalBus const& inputs, SignalBus&) override
    {
        (void)inputs;
        std::cout << "glb_z_reverse" << std::endl;
    }
};

} // namespace DSPatch