$input v_normal, v_color0

/*
 * Copyright 2018 Eric Arnebäck. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "../common/common.sh"

void main()
{
	vec3 color = v_color0.rgb * abs(v_normal.z);
	gl_FragColor = vec4(color, dot(color, vec3(0.2627, 0.6780, 0.0593)));
}
