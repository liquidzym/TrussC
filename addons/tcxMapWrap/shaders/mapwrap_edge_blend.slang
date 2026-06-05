// mapwrap_edge_blend.slang — Edge blend rendering
uniform fs_params {
    vec4 edge;       // left, top, right, bottom (normalized)
    vec3 gamma;
    float exponent;
    float brightness;
};

texture2D tex;
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
    out.gl_Position = vec4(in.position, 0.0, 1.0);
    out.uv = in.texcoord;
    return out;
}

frag_out fs_main(vs_out in) {
    vec4 color = texture(sampler2D(tex, smp), in.uv);
    // Edge blend calculation — P1 implementation
    color.rgb *= brightness;
    return color;
}
