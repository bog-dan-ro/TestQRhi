# QRhi Test Application

Requirements:
- Qt 6.9
- Linux

The goal is to create a Qt library which can be used to load the fantastic https://github.com/libretro/slang-shaders/ .
Sadly most of the shaders are using `layout(push_constant) uniform Push` which is not handled by Qt properly.
Is not a big issue, as `SPIRV-Cross` (used internal by `qsb`) converts it to a "normal" uniform.


## Issues

### qsb (SPIRV-Cross) conversion issues

There are two compile issues in `simple.frag/.vert` shaders.
Please open them and comment out the `#error ...` line.
Uncomment one of the `uniform Push` declarations.

Uncommenting `layout(std140, set = 0, binding = 1)  uniform Push` will make it work with QtRhi.
You'll see that the shader is correctly appiled.

Uncommenting `layout(push_constant) uniform Push` will sill ~work with QtRhi, but you'll see that the shader is NOT correctly appiled.
Because this uniform is not bound to index 1 anymore.

### :/simple.frag.qsb has params & global uniforms swapped 

For some reason which I don't udersnatd the position in the source file (printed by `getShader` function) `:/simple.frag.qsb` has params & global uniforms swapped.

**:/simple.vert.qsb** printed code:
```
#version 450

struct UBO
{
    mat4 MVP;
};

uniform UBO global;

struct Push
{
    vec4 SourceSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    uint FrameCount;
    float BIL_FALLBACK;
};

uniform Push params;
....

```

**:/simple.frag.qsb**  printed code:
```

#version 450

struct Push
{
    vec4 SourceSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    uint FrameCount;
    float BIL_FALLBACK;
};

uniform Push params;

struct UBO
{
    mat4 MVP;
};

uniform UBO global;
....
```


### Doesn't work using Vulkan backend of QtRhi
Setting `QSG_RHI_BACKEND=vulkan` env variabile will break the rendering even with super simple shaders that don't use `layout(push_constant)` e.g.:
```
// frag
#version 450

layout(location = 0) in vec2 v_texcoord;

layout(location = 0) out vec4 fragColor;

layout(binding = 2) uniform sampler2D tex;

void main()
{
    fragColor = texture(tex, v_texcoord);
}


// vert
#version 450

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform MVP {
    mat4 u_mvp;
};

void main() {
    v_texcoord = texcoord;
    gl_Position = u_mvp * position;
}
```
