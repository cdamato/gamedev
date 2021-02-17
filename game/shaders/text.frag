#version 120

uniform sampler2D tex;
uniform vec3 color;

varying vec2 uv_out;

void main() {
    gl_FragColor = vec4(color.rgb, texture2D(tex, uv_out).r);
};