#ifndef WORLD_GUNROTATION_H
#define WORLD_GUNROTATION_H

#include <cstdint>
#include "../matrix.h"
#include "../utils.h"

namespace World {

// Initial gun orientation shared by gameplay and visual-only attachments.
// Preserve the legacy normalization, vertical-axis and directDown conventions.
inline mat3x3 InitialGunRotation(const vec3d &_basis, bool directDown = false)
{
    vec3d basis = _basis;
    float ln = basis.length();

    if ( ln > 0.001 )
        basis /= ln;

    mat3x3 rotation;
    rotation.SetZ( basis );

    if ( basis.y != 0.0 )
    {
        if ( basis.x != 0.0 || basis.z != 0.0 )
        {
            float v12 = -1.0 / ( basis.y / basis.XZ().length() );

            rotation.m11 = sqrt(POW2(v12) / (POW2(v12) + 1.0));

            if ( basis.x != 0.0 )
            {
                float v14 = 1.0 - POW2(rotation.m11);
                rotation.m10 = sqrt( v14 / (POW2(basis.z) / (POW2(basis.x)) + 1.0) );
                rotation.m12 = sqrt( v14 - POW2(rotation.m10) );
            }
            else
            {
                float v17 = 1.0 - POW2(rotation.m11);
                rotation.m12 = sqrt( v17 / (POW2(basis.x) / (POW2(basis.z)) + 1.0) );
                rotation.m10 = sqrt( v17 - POW2(rotation.m12) );
            }

            if ( basis.x < 0.0 )
                rotation.m10 = -rotation.m10;

            if ( basis.z < 0.0 )
                rotation.m12 = -rotation.m12;

            if ( basis.y > 0.0 )
            {
                rotation.m10 = -rotation.m10;
                rotation.m12 = -rotation.m12;
            }
        }
        else
        {
            rotation.SetY( vec3d(0.0, 0.0, 1.0) );
        }
    }
    else
    {
        rotation.SetY( vec3d(0.0, 1.0, 0.0) );
    }

    if ( directDown )
    {
        rotation.SetY( -rotation.AxisY() );
    }

    rotation.SetX( rotation.AxisY() * rotation.AxisZ() );

    return rotation;
}

}

#endif
