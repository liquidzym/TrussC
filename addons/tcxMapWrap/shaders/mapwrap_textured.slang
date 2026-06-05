// mapwrap_textured.slang — Textured surface rendering
// Uses sokol-shdc cross-compilation for Metal / D3D11

uniform vs_params {
    mat4 mvp;
};

uniform fs_params {
    float opacity;
    float brightness;
};

texture2d tex;
sampler smp;

in vs_in {
    vec2 position;
    vec2 texcoord;
};

in vs_out {
    vec2 uv;
};

vs_out vs_main(vs_in in) {
    vs_out out;
    out.gl_Position = mvp * vec4(in.position, 0.0, 1.0);
    out.uv = in.texcoord;
    return out;
}

frag_out fs_main(vs_out in) {
    vec4 color = texture(sampler2D(tex, smp), in.uv);
    color.rgb *= brightness;
    color.a *= opacity;
    return color;
}
