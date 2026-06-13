// SPDX-FileCopyrightText: 2026 PCJohn (Jan Pečiva, peciva@fit.vut.cz)
//
// SPDX-License-Identifier: MIT-0

#version 460

// color primaries
vec2 primaries[] =
	vec2[](

		// invalid color space
		vec2(0, 0),
		vec2(0, 0),
		vec2(0, 0),

		// sRGB, BT709, extended sRGB: BT709 primaries (~35% of visible color spectrum)
		vec2(0.640, 0.330),
		vec2(0.300, 0.600),
		vec2(0.150, 0.060),

		// Display-P3, DCI-P3: P3 primaries (~45% of visible color spectrum)
		vec2(0.6800, 0.3200),
		vec2(0.2650, 0.6900),
		vec2(0.1500, 0.0600),

		// Adobe RGB: Adobe RGB primaries (~50% of visible color spectrum)
		vec2(0.6400, 0.3300),
		vec2(0.2100, 0.7100),
		vec2(0.1500, 0.0600),

		// BT2020, HDR10: BT2020 primaries (~75% of visible color spectrum)
		vec2(0.708, 0.292),
		vec2(0.170, 0.797),
		vec2(0.131, 0.046)

	);

vec4 colorTable[] =
	vec4[](
		vec4(1, 0, 0, 1),
		vec4(0, 1, 0, 1),
		vec4(0, 0, 1, 1)
	);

out gl_PerVertex {
	vec4 gl_Position;
};

layout(location = 0) out vec4 outColor;


void main()
{
	// get chromacity coordinates
	vec2 chroma = primaries[gl_VertexIndex];

	// vertex screen position
	gl_Position = vec4(chroma.x*2-1, -chroma.y*2+1, 0.5, 1.0);

	// color
	int colorIndex = gl_VertexIndex;
	outColor = colorTable[colorIndex % 3];
}
