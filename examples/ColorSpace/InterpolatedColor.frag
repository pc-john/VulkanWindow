// SPDX-FileCopyrightText: 2025 PCJohn (Jan Pečiva, peciva@fit.vut.cz)
//
// SPDX-License-Identifier: MIT-0

#version 450

layout(location = 0) in vec4 inColor;

layout(location = 0) out vec4 outColor;


void main()
{
	outColor = inColor;
}
