#version 120

uniform sampler2D tex;
varying vec2 uv_out;

void main()
{
    gl_FragColor = vec4(texture2D(tex, uv_out).rgba);
};