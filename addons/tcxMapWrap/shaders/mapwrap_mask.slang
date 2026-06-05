// mapwrap_mask.slang — Mask rendering
uniform fs_params {
    float mask_opacity;
    float feather;
};

in vs_in {
    vec2 position;
};

in vs_out {
    vec2 local_uv;
};

vs_out vs_main(vs_in in) {
    vs_out out;
    out.gl_Position = vec4(in.position, 0.0, 1.0);
    out.local_uv = in.position * 0.5 + 0.5;
    return out;
}

frag_out fs_main(vs_out in) {
    vec4 color = vec4(1.0, 1.0, 1.0, mask_opacity);
    return color;
}
