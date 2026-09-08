#ifndef CAMERA_INPUT_H
#define CAMERA_INPUT_H

#include <OISKeyboard.h>

//! Resolve held keys together so modifier changes and opposing keys are stable.
struct CameraInput
{
    float x = 0.0f;
    float y = 0.0f;
    float zoom = 0.0f;
    float swivel = 0.0f;
    bool fast = false;

    template<typename KeyDown>
    static CameraInput read(KeyDown down)
    {
        CameraInput input;
        const bool ctrl = down(OIS::KC_LCONTROL) || down(OIS::KC_RCONTROL);
        input.fast = down(OIS::KC_LSHIFT) || down(OIS::KC_RSHIFT);
        input.x = float(down(OIS::KC_D) || (!ctrl && down(OIS::KC_RIGHT)))
            - float(down(OIS::KC_A) || (!ctrl && down(OIS::KC_LEFT)));
        input.y = float(down(OIS::KC_W) || (!ctrl && down(OIS::KC_UP)))
            - float(down(OIS::KC_S) || (!ctrl && down(OIS::KC_DOWN)));
        input.zoom = float((!ctrl && down(OIS::KC_END)) || (ctrl && down(OIS::KC_DOWN)))
            - float((!ctrl && down(OIS::KC_HOME)) || (ctrl && down(OIS::KC_UP)));
        input.swivel = float(down(OIS::KC_Q) || (!ctrl && down(OIS::KC_DELETE)) || (ctrl && down(OIS::KC_LEFT)))
            - float(down(OIS::KC_E) || (!ctrl && down(OIS::KC_PGDOWN)) || (ctrl && down(OIS::KC_RIGHT)));
        return input;
    }
};

#endif
