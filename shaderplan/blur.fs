// blur.fs
#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec2 resolution;

void main() {
    vec2 tex_offset = 1.0 / resolution; // gets size of single texel
    vec3 result = texture(texture0, fragTexCoord).rgb * 0.227027;
    result += texture(texture0, fragTexCoord + vec2(tex_offset.x, 0.0)).rgb * 0.316216;
    result += texture(texture0, fragTexCoord - vec2(tex_offset.x, 0.0)).rgb * 0.316216;
    result += texture(texture0, fragTexCoord + vec2(0.0, tex_offset.y)).rgb * 0.070270;
    result += texture(texture0, fragTexCoord - vec2(0.0, tex_offset.y)).rgb * 0.070270;

    finalColor = vec4(result, 1.0);
}
