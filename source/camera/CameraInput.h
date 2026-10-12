#ifndef CAMERA_INPUT_H
#define CAMERA_INPUT_H

#include <OISKeyboard.h>

//! Resolve held keys together so modifier changes and opposing keys are stable.
struct CameraInput
{
    //! Pan to the right (1) or left (-1).
    float x = 0.0f;
    //! Pan forward (1) or backward (-1).
    float y = 0.0f;
    //! Zoom out (1) or in (-1).
    float zoom = 0.0f;
    //! Swivel to the left (1) or right (-1).
    float swivel = 0.0f;
    //! Whether the fast modifier key is held.
    bool fast = false;

    //! \brief Reads the held keys. With Ctrl held, the arrow keys swivel and zoom instead of panning.
    //! \param down Returns whether the given key is held.
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
