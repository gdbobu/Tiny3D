#include "shader/util.glsl"

layout(bindless_sampler) uniform sampler2D colorBuffer;
uniform vec2 pixelSize;
uniform float pass;

in vec2 vTexcoord;

out vec4 FragColor;

const float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
	vec4 result = texture(colorBuffer, vTexcoord) * weight[0];
    if(pass < 1.5) {
        for(int i = 1; i < 5; ++i) {
            result += texture(colorBuffer, vTexcoord + vec2(pixelSize.x * i, 0.0)) * weight[i];
            result += texture(colorBuffer, vTexcoord - vec2(pixelSize.x * i, 0.0)) * weight[i];
        }
    } else {
        for(int i = 1; i < 5; ++i) {
            result += texture(colorBuffer, vTexcoord + vec2(0.0, pixelSize.y * i)) * weight[i];
            result += texture(colorBuffer, vTexcoord - vec2(0.0, pixelSize.y * i)) * weight[i];
        }
    }
    FragColor = result;
}