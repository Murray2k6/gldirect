/* Runtime ABI smoke test for an arbitrary GLDirect opengl32.dll build. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned char GLubyte;

#define GL_VERSION 0x1F02u
#define GL_COLOR_BUFFER_BIT 0x00004000u
#define GL_COMPUTE_SHADER 0x91B9u
#define GL_VERTEX_SHADER 0x8B31u
#define GL_GEOMETRY_SHADER 0x8DD9u
#define GL_TESS_CONTROL_SHADER 0x8E88u
#define GL_TESS_EVALUATION_SHADER 0x8E87u
#define GL_FRAGMENT_SHADER 0x8B30u
#define GL_COMPILE_STATUS 0x8B81u
#define GL_LINK_STATUS 0x8B82u
#define GL_SHADER_STORAGE_BUFFER 0x90D2u
#define GL_DYNAMIC_COPY 0x88EAu
#define GL_ALL_BARRIER_BITS 0xFFFFFFFFu
#define GL_TEXTURE_2D 0x0DE1u
#define GL_TEXTURE_BUFFER 0x8C2Au
#define GL_RGBA 0x1908u
#define GL_RGBA8 0x8058u
#define GL_R32F 0x822Eu
#define GL_UNSIGNED_BYTE 0x1401u
#define GL_FLOAT 0x1406u
#define GL_WRITE_ONLY 0x88B9u
#define GL_POINTS 0x0000u
#define GL_TRIANGLES 0x0004u
#define GL_PATCHES 0x000Eu
#define GL_TRANSFORM_FEEDBACK_BUFFER 0x8C8Eu
#define GL_INTERLEAVED_ATTRIBS 0x8C8Cu
#define GL_RASTERIZER_DISCARD 0x8C89u
#define GL_COMPILE 0x1300u
#define GL_CURRENT_COLOR 0x0B00u

typedef int   (WINAPI *PFN_CHOOSE_PIXEL_FORMAT)(HDC, const PIXELFORMATDESCRIPTOR *);
typedef BOOL  (WINAPI *PFN_SET_PIXEL_FORMAT)(HDC, int, const PIXELFORMATDESCRIPTOR *);
typedef HGLRC (WINAPI *PFN_CREATE_CONTEXT)(HDC);
typedef BOOL  (WINAPI *PFN_DELETE_CONTEXT)(HGLRC);
typedef BOOL  (WINAPI *PFN_MAKE_CURRENT)(HDC, HGLRC);
typedef PROC  (WINAPI *PFN_GET_PROC_ADDRESS)(LPCSTR);
typedef BOOL  (WINAPI *PFN_SWAP_BUFFERS)(HDC);
typedef HGLRC (WINAPI *PFN_CREATE_CONTEXT_ATTRIBS)(HDC, HGLRC, const int *);
typedef const GLubyte *(WINAPI *PFN_GET_STRING)(GLenum);
typedef void  (WINAPI *PFN_CLEAR_COLOR)(float, float, float, float);
typedef void  (WINAPI *PFN_CLEAR)(GLbitfield);
typedef unsigned int (WINAPI *PFN_CREATE_SHADER)(GLenum);
typedef void  (WINAPI *PFN_SHADER_SOURCE)(unsigned int, int,
                                          const char *const *, const int *);
typedef void  (WINAPI *PFN_COMPILE_SHADER)(unsigned int);
typedef void  (WINAPI *PFN_GET_SHADER_IV)(unsigned int, GLenum, int *);
typedef void  (WINAPI *PFN_DELETE_SHADER)(unsigned int);
typedef unsigned int (WINAPI *PFN_CREATE_PROGRAM)(void);
typedef void  (WINAPI *PFN_ATTACH_SHADER)(unsigned int, unsigned int);
typedef void  (WINAPI *PFN_LINK_PROGRAM)(unsigned int);
typedef void  (WINAPI *PFN_GET_PROGRAM_IV)(unsigned int, GLenum, int *);
typedef void  (WINAPI *PFN_GET_PROGRAM_INFO_LOG)(unsigned int, int, int *, char *);
typedef void  (WINAPI *PFN_USE_PROGRAM)(unsigned int);
typedef void  (WINAPI *PFN_DELETE_PROGRAM)(unsigned int);
typedef void  (WINAPI *PFN_GEN_BUFFERS)(int, unsigned int *);
typedef void  (WINAPI *PFN_BIND_BUFFER)(GLenum, unsigned int);
typedef void  (WINAPI *PFN_BUFFER_DATA)(GLenum, size_t, const void *, GLenum);
typedef void  (WINAPI *PFN_BIND_BUFFER_BASE)(GLenum, unsigned int, unsigned int);
typedef void  (WINAPI *PFN_GET_BUFFER_SUB_DATA)(GLenum, size_t, size_t, void *);
typedef void  (WINAPI *PFN_DELETE_BUFFERS)(int, const unsigned int *);
typedef void  (WINAPI *PFN_DISPATCH_COMPUTE)(unsigned int, unsigned int, unsigned int);
typedef void  (WINAPI *PFN_MEMORY_BARRIER)(GLbitfield);
typedef void  (WINAPI *PFN_GEN_TEXTURES)(int, unsigned int *);
typedef void  (WINAPI *PFN_BIND_TEXTURE)(GLenum, unsigned int);
typedef void  (WINAPI *PFN_TEX_IMAGE_2D)(GLenum, int, int, int, int, int, GLenum, GLenum, const void *);
typedef void  (WINAPI *PFN_TEX_BUFFER_RANGE)(GLenum, GLenum, unsigned int, size_t, size_t);
typedef void  (WINAPI *PFN_BIND_IMAGE_TEXTURE)(unsigned int, unsigned int, int, unsigned char, int, GLenum, GLenum);
typedef void  (WINAPI *PFN_GET_TEX_IMAGE)(GLenum, int, GLenum, GLenum, void *);
typedef void  (WINAPI *PFN_DELETE_TEXTURES)(int, const unsigned int *);
typedef unsigned int (WINAPI *PFN_GEN_LISTS)(int);
typedef void  (WINAPI *PFN_NEW_LIST)(unsigned int, GLenum);
typedef void  (WINAPI *PFN_END_LIST)(void);
typedef void  (WINAPI *PFN_CALL_LIST)(unsigned int);
typedef void  (WINAPI *PFN_DELETE_LISTS)(unsigned int, int);
typedef void  (WINAPI *PFN_COLOR_4F)(float, float, float, float);
typedef void  (WINAPI *PFN_GET_FLOATV)(GLenum, float *);
typedef void  (WINAPI *PFN_DRAW_ARRAYS)(GLenum, int, int);
typedef void  (WINAPI *PFN_VIEWPORT)(int, int, int, int);
typedef void  (WINAPI *PFN_READ_PIXELS)(int, int, int, int, GLenum, GLenum, void *);
typedef GLenum (WINAPI *PFN_GET_ERROR)(void);
typedef void  (WINAPI *PFN_DRAW_ARRAYS_INSTANCED)(GLenum, int, int, int);
typedef void  (WINAPI *PFN_VERTEX_ATTRIB_POINTER)(unsigned int, int, GLenum,
                                                   unsigned char, int,
                                                   const void *);
typedef void  (WINAPI *PFN_VERTEX_ATTRIB_ARRAY)(unsigned int);
typedef void  (WINAPI *PFN_TRANSFORM_FEEDBACK_VARYINGS)(unsigned int, int,
                                                         const char *const *, GLenum);
typedef void  (WINAPI *PFN_BEGIN_TRANSFORM_FEEDBACK)(GLenum);
typedef void  (WINAPI *PFN_END_TRANSFORM_FEEDBACK)(void);
typedef void  (WINAPI *PFN_ENABLE_DISABLE)(GLenum);
typedef const char *(WINAPI *PFN_GET_EXTENSIONS_STRING_ARB)(HDC);
typedef BOOL  (WINAPI *PFN_CHOOSE_PIXEL_FORMAT_ARB)(HDC, const int *,
                                                    const FLOAT *, UINT,
                                                    int *, UINT *);
typedef BOOL  (WINAPI *PFN_GET_PIXEL_FORMAT_ATTRIB_IV_ARB)(HDC, int, int, UINT,
                                                           const int *, int *);

/* WGL_ARB_pixel_format */
#define WGL_NUMBER_PIXEL_FORMATS_ARB 0x2000
#define WGL_DRAW_TO_WINDOW_ARB       0x2001
#define WGL_ACCELERATION_ARB         0x2003
#define WGL_SUPPORT_OPENGL_ARB       0x2010
#define WGL_DOUBLE_BUFFER_ARB        0x2011
#define WGL_PIXEL_TYPE_ARB           0x2013
#define WGL_COLOR_BITS_ARB           0x2014
#define WGL_ALPHA_BITS_ARB           0x201B
#define WGL_DEPTH_BITS_ARB           0x2022
#define WGL_STENCIL_BITS_ARB         0x2023
#define WGL_FULL_ACCELERATION_ARB    0x2027
#define WGL_TYPE_RGBA_ARB            0x202B
#define WGL_SAMPLE_BUFFERS_ARB       0x2041
#define WGL_SAMPLES_ARB              0x2042

static LRESULT CALLBACK smokeWindowProc(HWND window, UINT message,
                                         WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(window, message, wParam, lParam);
}

static FARPROC required(HMODULE module, const char *name)
{
    FARPROC proc = GetProcAddress(module, name);
    if (!proc)
        fprintf(stderr, "missing export: %s (error %lu)\n",
                name, (unsigned long)GetLastError());
    return proc;
}

int main(int argc, char **argv)
{
    WNDCLASSEXA wc;
    PIXELFORMATDESCRIPTOR pfd;
    HINSTANCE instance = GetModuleHandleA(NULL);
    HMODULE wrapper;
    HWND window = NULL;
    HDC dc = NULL;
    HGLRC context = NULL;
    ATOM windowClass = 0;
    int format;
    int result = 1;
    PFN_CHOOSE_PIXEL_FORMAT choosePixelFormat;
    PFN_SET_PIXEL_FORMAT setPixelFormat;
    PFN_CREATE_CONTEXT createContext;
    PFN_DELETE_CONTEXT deleteContext;
    PFN_MAKE_CURRENT makeCurrent;
    PFN_GET_PROC_ADDRESS getProcAddress;
    PFN_SWAP_BUFFERS swapBuffers;
    PFN_GET_STRING getString;
    PFN_CLEAR_COLOR clearColor;
    PFN_CLEAR clear;
    PFN_CREATE_CONTEXT_ATTRIBS createContextAttribs;
    const GLubyte *version;

    if (argc != 2) {
        fprintf(stderr, "usage: wgl_smoke.exe <absolute-opengl32.dll-path>\n");
        return 2;
    }

    wrapper = LoadLibraryA(argv[1]);
    if (!wrapper) {
        fprintf(stderr, "LoadLibrary failed: %lu\n", (unsigned long)GetLastError());
        return 3;
    }

    choosePixelFormat = (PFN_CHOOSE_PIXEL_FORMAT)required(wrapper, "wglChoosePixelFormat");
    setPixelFormat = (PFN_SET_PIXEL_FORMAT)required(wrapper, "wglSetPixelFormat");
    createContext = (PFN_CREATE_CONTEXT)required(wrapper, "wglCreateContext");
    deleteContext = (PFN_DELETE_CONTEXT)required(wrapper, "wglDeleteContext");
    makeCurrent = (PFN_MAKE_CURRENT)required(wrapper, "wglMakeCurrent");
    getProcAddress = (PFN_GET_PROC_ADDRESS)required(wrapper, "wglGetProcAddress");
    swapBuffers = (PFN_SWAP_BUFFERS)required(wrapper, "wglSwapBuffers");
    getString = (PFN_GET_STRING)required(wrapper, "glGetString");
    clearColor = (PFN_CLEAR_COLOR)required(wrapper, "glClearColor");
    clear = (PFN_CLEAR)required(wrapper, "glClear");
    if (!choosePixelFormat || !setPixelFormat || !createContext ||
        !deleteContext || !makeCurrent || !getProcAddress || !swapBuffers ||
        !getString || !clearColor || !clear)
        goto cleanup;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = smokeWindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = "GLDirectWglSmoke";
    windowClass = RegisterClassExA(&wc);
    if (!windowClass) {
        fprintf(stderr, "RegisterClassEx failed: %lu\n", (unsigned long)GetLastError());
        goto cleanup;
    }

    window = CreateWindowExA(0, wc.lpszClassName, "GLDirect smoke",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             320, 240, NULL, NULL, instance, NULL);
    if (!window) {
        fprintf(stderr, "CreateWindowEx failed: %lu\n", (unsigned long)GetLastError());
        goto cleanup;
    }
    ShowWindow(window, SW_SHOWNA);
    UpdateWindow(window);
    dc = GetDC(window);
    if (!dc)
        goto cleanup;

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    format = choosePixelFormat(dc, &pfd);
    if (format <= 0 || !setPixelFormat(dc, format, &pfd)) {
        fprintf(stderr, "pixel format setup failed: format=%d error=%lu\n",
                format, (unsigned long)GetLastError());
        goto cleanup;
    }

    context = createContext(dc);
    if (!context || !makeCurrent(dc, context)) {
        fprintf(stderr, "context setup failed: error=%lu\n",
                (unsigned long)GetLastError());
        goto cleanup;
    }

    createContextAttribs =
        (PFN_CREATE_CONTEXT_ATTRIBS)getProcAddress("wglCreateContextAttribsARB");
    if (createContextAttribs) {
        static const int core46Attributes[] = {
            0x2091, 4,       /* WGL_CONTEXT_MAJOR_VERSION_ARB */
            0x2092, 6,       /* WGL_CONTEXT_MINOR_VERSION_ARB */
            0x9126, 0x0001,  /* WGL_CONTEXT_PROFILE_MASK_ARB / CORE */
            0
        };
        HGLRC coreContext = createContextAttribs(dc, NULL, core46Attributes);
        if (coreContext) {
            if (!makeCurrent(dc, coreContext)) {
                deleteContext(coreContext);
                fprintf(stderr, "making the 4.6 core context current failed\n");
                goto cleanup;
            }
            deleteContext(context);
            context = coreContext;
        }
    }

    /*
     * WGL_ARB_pixel_format. Applications that negotiate a modern context
     * pick their format through this path and treat a missing entry point
     * or an empty result as fatal.
     */
    {
        PFN_GET_EXTENSIONS_STRING_ARB getExtensionsStringARB =
            (PFN_GET_EXTENSIONS_STRING_ARB)getProcAddress("wglGetExtensionsStringARB");
        PFN_CHOOSE_PIXEL_FORMAT_ARB choosePixelFormatARB =
            (PFN_CHOOSE_PIXEL_FORMAT_ARB)getProcAddress("wglChoosePixelFormatARB");
        PFN_GET_PIXEL_FORMAT_ATTRIB_IV_ARB getPixelFormatAttribivARB =
            (PFN_GET_PIXEL_FORMAT_ATTRIB_IV_ARB)getProcAddress("wglGetPixelFormatAttribivARB");
        const char *wglExtensions;
        static const int formatAttributes[] = {
            WGL_DRAW_TO_WINDOW_ARB, TRUE,
            WGL_SUPPORT_OPENGL_ARB, TRUE,
            WGL_DOUBLE_BUFFER_ARB,  TRUE,
            WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
            WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
            WGL_COLOR_BITS_ARB,     32,
            WGL_DEPTH_BITS_ARB,     24,
            WGL_STENCIL_BITS_ARB,   8,
            0
        };
        /* Multisampling is not enumerated; the request must still yield a
         * format instead of an empty result the application treats as
         * fatal. */
        static const int multisampleAttributes[] = {
            WGL_DRAW_TO_WINDOW_ARB, TRUE,
            WGL_SUPPORT_OPENGL_ARB, TRUE,
            WGL_DOUBLE_BUFFER_ARB,  TRUE,
            WGL_COLOR_BITS_ARB,     32,
            WGL_DEPTH_BITS_ARB,     24,
            WGL_SAMPLE_BUFFERS_ARB, 1,
            WGL_SAMPLES_ARB,        4,
            0
        };
        static const int queried[] = {
            WGL_NUMBER_PIXEL_FORMATS_ARB,
            WGL_SUPPORT_OPENGL_ARB,
            WGL_DOUBLE_BUFFER_ARB,
            WGL_PIXEL_TYPE_ARB,
            WGL_ACCELERATION_ARB,
            WGL_COLOR_BITS_ARB,
            WGL_ALPHA_BITS_ARB,
            WGL_DEPTH_BITS_ARB,
            WGL_STENCIL_BITS_ARB,
            WGL_SAMPLES_ARB
        };
        int arbFormats[8];
        int values[sizeof(queried) / sizeof(queried[0])];
        UINT formatCount = 0;

        if (!getExtensionsStringARB) {
            fprintf(stderr, "wglGetExtensionsStringARB did not resolve\n");
            goto cleanup;
        }
        wglExtensions = getExtensionsStringARB(dc);
        if (!wglExtensions || !strstr(wglExtensions, "WGL_ARB_pixel_format")) {
            fprintf(stderr, "WGL_ARB_pixel_format is not advertised: %s\n",
                    wglExtensions ? wglExtensions : "(null)");
            goto cleanup;
        }
        if (!choosePixelFormatARB || !getPixelFormatAttribivARB) {
            fprintf(stderr, "WGL_ARB_pixel_format entry points did not resolve "
                            "(choose=%p attribiv=%p)\n",
                    (void *)choosePixelFormatARB,
                    (void *)getPixelFormatAttribivARB);
            goto cleanup;
        }

        if (!choosePixelFormatARB(dc, formatAttributes, NULL, 8, arbFormats,
                                  &formatCount) || formatCount == 0) {
            fprintf(stderr, "wglChoosePixelFormatARB returned no format\n");
            goto cleanup;
        }
        if (arbFormats[0] <= 0) {
            fprintf(stderr, "wglChoosePixelFormatARB returned invalid format %d\n",
                    arbFormats[0]);
            goto cleanup;
        }

        if (!getPixelFormatAttribivARB(dc, arbFormats[0], 0,
                                       sizeof(queried) / sizeof(queried[0]),
                                       queried, values)) {
            fprintf(stderr, "wglGetPixelFormatAttribivARB failed for format %d\n",
                    arbFormats[0]);
            goto cleanup;
        }
        if (values[0] < arbFormats[0]) {
            fprintf(stderr, "format %d outside the reported count %d\n",
                    arbFormats[0], values[0]);
            goto cleanup;
        }
        if (values[1] != TRUE || values[2] != TRUE ||
            values[3] != WGL_TYPE_RGBA_ARB ||
            values[4] != WGL_FULL_ACCELERATION_ARB) {
            fprintf(stderr, "chosen format reports the wrong capabilities: "
                            "opengl=%d doublebuffer=%d type=0x%X accel=0x%X\n",
                    values[1], values[2], values[3], values[4]);
            goto cleanup;
        }
        if (values[5] < 32 || values[7] < 24 || values[8] < 8) {
            fprintf(stderr, "chosen format is smaller than requested: "
                            "color=%d depth=%d stencil=%d\n",
                    values[5], values[7], values[8]);
            goto cleanup;
        }

        formatCount = 0;
        if (!choosePixelFormatARB(dc, multisampleAttributes, NULL, 8,
                                  arbFormats, &formatCount) ||
            formatCount == 0 || arbFormats[0] <= 0) {
            fprintf(stderr, "wglChoosePixelFormatARB failed to degrade an "
                            "unsupported multisample request\n");
            goto cleanup;
        }
    }

    version = getString(GL_VERSION);
    if (!version || !version[0]) {
        fprintf(stderr, "glGetString(GL_VERSION) returned no value\n");
        goto cleanup;
    }
    if (getProcAddress("glCreateShader") == NULL) {
        fprintf(stderr, "glCreateShader did not resolve\n");
        goto cleanup;
    }
    if (getProcAddress("glDefinitelyNotARealEntryPointGLDIRECT") != NULL) {
        fprintf(stderr, "unknown GL entry point incorrectly resolved\n");
        goto cleanup;
    }

    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog = (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_DRAW_ARRAYS drawArrays = (PFN_DRAW_ARRAYS)getProcAddress("glDrawArrays");
        PFN_VIEWPORT viewport = (PFN_VIEWPORT)required(wrapper, "glViewport");
        PFN_READ_PIXELS readPixels = (PFN_READ_PIXELS)required(wrapper, "glReadPixels");
        PFN_GET_ERROR getError = (PFN_GET_ERROR)required(wrapper, "glGetError");
        static const char *vsSource =
            "#version 460 core\n"
            "void main(){ gl_Position=vec4(0.0,0.0,0.0,1.0); }\n";
        static const char *gsSource =
            "#version 460 core\n"
            "layout(points) in; layout(triangle_strip,max_vertices=3) out;\n"
            "out vec4 stageColor;\n"
            "void main(){ stageColor=vec4(1.0,0.0,0.0,1.0);\n"
            "gl_Position=vec4(-0.75,-0.75,0.0,1.0); EmitVertex();\n"
            "gl_Position=vec4( 0.75,-0.75,0.0,1.0); EmitVertex();\n"
            "gl_Position=vec4( 0.00, 0.75,0.0,1.0); EmitVertex(); EndPrimitive(); }\n";
        static const char *fsSource =
            "#version 460 core\n"
            "in vec4 stageColor; layout(location=0) out vec4 finalColor;\n"
            "void main(){ finalColor=stageColor; }\n";
        const GLenum shaderTypes[3] = { GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
        const char *sources[3] = { vsSource, gsSource, fsSource };
        unsigned int shaders[3], program;
        unsigned char pixel[4] = { 0, 0, 0, 0 };
        int i, ok, linked;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog || !useProgram || !deleteProgram ||
            !drawArrays || !viewport || !readPixels || !getError)
            goto cleanup;
        program = createProgram();
        for (i = 0; i < 3; ++i) {
            shaders[i] = createShader(shaderTypes[i]);
            shaderSource(shaders[i], 1, &sources[i], NULL);
            compileShader(shaders[i]);
            ok = 0;
            getShaderiv(shaders[i], GL_COMPILE_STATUS, &ok);
            if (!ok) {
                fprintf(stderr, "programmable stage %d did not compile\n", i);
                goto cleanup;
            }
            attachShader(program, shaders[i]);
        }
        linkProgram(program);
        linked = 0;
        getProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[1024]; int logLength = 0;
            getProgramInfoLog(program, sizeof(log), &logLength, log);
            fprintf(stderr, "geometry program did not link: %s\n", log);
            goto cleanup;
        }
        /* Deliberately larger than the client area: GL permits this, while
         * D3D9 requires a clipped viewport plus clip-space compensation. */
        viewport(0, 0, 320, 240);
        clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        clear(GL_COLOR_BUFFER_BIT);
        useProgram(program);
        drawArrays(GL_POINTS, 0, 1);
        useProgram(0);
        if (getError() != 0) {
            fprintf(stderr, "geometry-stage draw reported a GL error\n");
            goto cleanup;
        }
        {
            RECT client;
            GetClientRect(window, &client);
            readPixels(client.right / 2, client.bottom / 2, 1, 1,
                       GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        }
        for (i = 0; i < 3; ++i) deleteShader(shaders[i]);
        deleteProgram(program);
        if (pixel[0] < 200 || pixel[1] > 32 || pixel[2] > 32) {
            RECT client = {0, 0, 0, 0};
            unsigned char *frame = NULL;
            int redCount = 0, minX = 0, minY = 0, maxX = 0, maxY = 0;
            if (GetClientRect(window, &client) && client.right > 0 && client.bottom > 0) {
                int x, y;
                size_t frameBytes = (size_t)client.right * (size_t)client.bottom * 4u;
                frame = (unsigned char *)malloc(frameBytes);
                if (frame) {
                    readPixels(0, 0, client.right, client.bottom,
                               GL_RGBA, GL_UNSIGNED_BYTE, frame);
                    for (y = 0; y < client.bottom; ++y) for (x = 0; x < client.right; ++x) {
                        const unsigned char *p = frame +
                            ((size_t)y * (size_t)client.right + (size_t)x) * 4u;
                        if (p[0] >= 200 && p[1] <= 32 && p[2] <= 32) {
                            if (!redCount) minX = maxX = x, minY = maxY = y;
                            if (x < minX) minX = x; if (x > maxX) maxX = x;
                            if (y < minY) minY = y; if (y > maxY) maxY = y;
                            ++redCount;
                        }
                    }
                    free(frame);
                }
            }
            fprintf(stderr, "geometry-stage D3D9 handoff returned RGBA=(%u,%u,%u,%u)\n",
                    pixel[0], pixel[1], pixel[2], pixel[3]);
            fprintf(stderr, "red framebuffer pixels=%d bounds=(%d,%d)-(%d,%d) client=%ldx%ld\n",
                    redCount, minX, minY, maxX, maxY,
                    (long)client.right, (long)client.bottom);
            goto cleanup;
        }
    }

    /* Ordinary translated VS/PS path: validates GL clip-depth conversion,
     * half-pixel placement and oversized-viewport compensation in SM3. */
    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog = (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_DRAW_ARRAYS drawArrays = (PFN_DRAW_ARRAYS)getProcAddress("glDrawArrays");
        PFN_VERTEX_ATTRIB_POINTER vertexAttribPointer =
            (PFN_VERTEX_ATTRIB_POINTER)getProcAddress("glVertexAttribPointer");
        PFN_VERTEX_ATTRIB_ARRAY enableVertexAttribArray =
            (PFN_VERTEX_ATTRIB_ARRAY)getProcAddress("glEnableVertexAttribArray");
        PFN_VERTEX_ATTRIB_ARRAY disableVertexAttribArray =
            (PFN_VERTEX_ATTRIB_ARRAY)getProcAddress("glDisableVertexAttribArray");
        PFN_VIEWPORT viewport = (PFN_VIEWPORT)required(wrapper, "glViewport");
        PFN_READ_PIXELS readPixels = (PFN_READ_PIXELS)required(wrapper, "glReadPixels");
        static const char *vsSource =
            "#version 460 core\nlayout(location=0) in vec3 position;\n"
            "void main(){ gl_Position=vec4(position,1.0); }\n";
        static const char *fsSource =
            "#version 460 core\nlayout(location=0) out vec4 finalColor;\n"
            "void main(){ finalColor=vec4(1.0,1.0,0.0,1.0); }\n";
        const float positions[9] = {
            -0.75f, -0.75f, -0.5f,
             0.75f, -0.75f, -0.5f,
             0.00f,  0.75f, -0.5f
        };
        unsigned int vs, fs, program;
        unsigned char pixel[4] = {0, 0, 0, 0};
        int ok = 0;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog || !useProgram || !deleteProgram ||
            !drawArrays || !vertexAttribPointer || !enableVertexAttribArray ||
            !disableVertexAttribArray || !viewport || !readPixels) goto cleanup;
        vs = createShader(GL_VERTEX_SHADER); shaderSource(vs, 1, &vsSource, NULL);
        compileShader(vs); getShaderiv(vs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "translated vertex shader did not compile\n"); goto cleanup; }
        fs = createShader(GL_FRAGMENT_SHADER); shaderSource(fs, 1, &fsSource, NULL);
        compileShader(fs); getShaderiv(fs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "translated fragment shader did not compile\n"); goto cleanup; }
        program = createProgram(); attachShader(program, vs); attachShader(program, fs);
        linkProgram(program); getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024]; int length = 0;
            getProgramInfoLog(program, sizeof(log), &length, log);
            fprintf(stderr, "translated VS/PS program did not link: %s\n", log);
            goto cleanup;
        }
        viewport(0, 0, 320, 240);
        clearColor(0, 0, 0, 1); clear(GL_COLOR_BUFFER_BIT);
        vertexAttribPointer(0, 3, GL_FLOAT, 0, 3 * (int)sizeof(float), positions);
        enableVertexAttribArray(0);
        useProgram(program); drawArrays(GL_TRIANGLES, 0, 3); useProgram(0);
        disableVertexAttribArray(0);
        readPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        deleteShader(vs); deleteShader(fs); deleteProgram(program);
        if (pixel[0] < 200 || pixel[1] < 200 || pixel[2] > 32) {
            fprintf(stderr, "translated VS/PS viewport path returned RGBA=(%u,%u,%u,%u)\n",
                    pixel[0], pixel[1], pixel[2], pixel[3]);
            goto cleanup;
        }
    }

    /* Sampler dimensionality must survive generic texture()/textureLod()
     * lowering; treating every sampler as 2D makes otherwise valid cube and
     * volume shaders fail at D3DCompile time. */
    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog =
            (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        static const char *vsSource =
            "#version 460 core\nlayout(location=0) in vec3 position;\n"
            "void main(){gl_Position=vec4(position,1.0);}\n";
        static const char *fsSource =
            "#version 460 core\nuniform samplerCube cubeMap; uniform sampler3D volumeMap;\n"
            "layout(location=0) out vec4 finalColor;\n"
            "void main(){finalColor=textureLod(cubeMap,vec3(1,0,0),0.0)+"
            "texture(volumeMap,vec3(0.5))*0.001;}\n";
        unsigned int vs, fs, program;
        int ok = 0;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog || !deleteProgram) goto cleanup;
        vs = createShader(GL_VERTEX_SHADER); shaderSource(vs, 1, &vsSource, NULL);
        compileShader(vs); getShaderiv(vs, GL_COMPILE_STATUS, &ok); if (!ok) goto cleanup;
        fs = createShader(GL_FRAGMENT_SHADER); shaderSource(fs, 1, &fsSource, NULL);
        compileShader(fs); getShaderiv(fs, GL_COMPILE_STATUS, &ok); if (!ok) goto cleanup;
        program = createProgram(); attachShader(program, vs); attachShader(program, fs);
        linkProgram(program); getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024]; int length = 0;
            getProgramInfoLog(program, sizeof(log), &length, log);
            fprintf(stderr, "cube/volume sampler program did not link: %s\n", log);
            goto cleanup;
        }
        deleteShader(vs); deleteShader(fs); deleteProgram(program);
    }

    /* A fragment operation with no Shader Model 3 representation executes in
     * the private GL 4.6 raster path and is copied back to the D3D9 target. */
    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog =
            (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_DRAW_ARRAYS drawArrays = (PFN_DRAW_ARRAYS)getProcAddress("glDrawArrays");
        PFN_VERTEX_ATTRIB_POINTER vertexAttribPointer =
            (PFN_VERTEX_ATTRIB_POINTER)getProcAddress("glVertexAttribPointer");
        PFN_VERTEX_ATTRIB_ARRAY enableVertexAttribArray =
            (PFN_VERTEX_ATTRIB_ARRAY)getProcAddress("glEnableVertexAttribArray");
        PFN_VERTEX_ATTRIB_ARRAY disableVertexAttribArray =
            (PFN_VERTEX_ATTRIB_ARRAY)getProcAddress("glDisableVertexAttribArray");
        PFN_VIEWPORT viewport = (PFN_VIEWPORT)required(wrapper, "glViewport");
        PFN_READ_PIXELS readPixels = (PFN_READ_PIXELS)required(wrapper, "glReadPixels");
        static const char *vsSource =
            "#version 460 core\nlayout(location=0) in vec2 position;\n"
            "void main(){gl_Position=vec4(position,0.0,1.0);}\n";
        static const char *fsSource =
            "#version 460 core\nlayout(location=0) out vec4 finalColor;\n"
            "void main(){uint value=(5u<<1u)^3u;"
            "finalColor=(value==9u)?vec4(0.0,0.0,1.0,1.0):vec4(1.0,0.0,0.0,1.0);}\n";
        const float positions[6] = {
            -0.75f, -0.75f, 0.75f, -0.75f, 0.0f, 0.75f
        };
        unsigned int vs, fs, program;
        unsigned char pixel[4] = {0, 0, 0, 0};
        int ok = 0;
        RECT client;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog || !useProgram || !deleteProgram ||
            !drawArrays || !vertexAttribPointer || !enableVertexAttribArray ||
            !disableVertexAttribArray || !viewport || !readPixels) goto cleanup;
        vs = createShader(GL_VERTEX_SHADER);
        shaderSource(vs, 1, &vsSource, NULL); compileShader(vs);
        getShaderiv(vs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "software-fragment vertex shader did not compile\n"); goto cleanup; }
        fs = createShader(GL_FRAGMENT_SHADER);
        shaderSource(fs, 1, &fsSource, NULL); compileShader(fs);
        getShaderiv(fs, GL_COMPILE_STATUS, &ok);
        if (!ok) { fprintf(stderr, "software-fragment shader did not compile\n"); goto cleanup; }
        program = createProgram(); attachShader(program, vs); attachShader(program, fs);
        linkProgram(program); getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024]; int length = 0;
            getProgramInfoLog(program, sizeof(log), &length, log);
            fprintf(stderr, "software-fragment program did not link: %s\n", log);
            goto cleanup;
        }
        GetClientRect(window, &client);
        viewport(0, 0, client.right, client.bottom);
        clearColor(0.0f, 0.0f, 0.0f, 1.0f); clear(GL_COLOR_BUFFER_BIT);
        vertexAttribPointer(0, 2, GL_FLOAT, 0, 2 * (int)sizeof(float), positions);
        enableVertexAttribArray(0);
        useProgram(program); drawArrays(GL_TRIANGLES, 0, 3); useProgram(0);
        disableVertexAttribArray(0);
        readPixels(client.right / 2, client.bottom / 2, 1, 1,
                   GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        deleteShader(vs); deleteShader(fs); deleteProgram(program);
        if (pixel[2] < 200 || pixel[0] > 32 || pixel[1] > 32) {
            fprintf(stderr, "software-fragment path returned RGBA=(%u,%u,%u,%u)\n",
                    pixel[0], pixel[1], pixel[2], pixel[3]);
            goto cleanup;
        }
    }

    /* Tessellation control/evaluation execute in the private GL 4.6 stage
     * worker; their generated triangle is then rasterized by D3D9. */
    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog = (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_DRAW_ARRAYS drawArrays = (PFN_DRAW_ARRAYS)getProcAddress("glDrawArrays");
        PFN_VIEWPORT viewport = (PFN_VIEWPORT)required(wrapper, "glViewport");
        PFN_READ_PIXELS readPixels = (PFN_READ_PIXELS)required(wrapper, "glReadPixels");
        static const char *vsSource =
            "#version 460 core\n"
            "const vec2 p[3]=vec2[3](vec2(-0.7,-0.7),vec2(0.7,-0.7),vec2(0.0,0.7));\n"
            "void main(){ gl_Position=vec4(p[gl_VertexID],0.0,1.0); }\n";
        static const char *tcsSource =
            "#version 460 core\nlayout(vertices=3) out;\n"
            "void main(){ gl_out[gl_InvocationID].gl_Position=gl_in[gl_InvocationID].gl_Position;"
            "if(gl_InvocationID==0){ gl_TessLevelOuter[0]=1.0; gl_TessLevelOuter[1]=1.0;"
            "gl_TessLevelOuter[2]=1.0; gl_TessLevelInner[0]=1.0; } }\n";
        static const char *tesSource =
            "#version 460 core\nlayout(triangles,equal_spacing,ccw) in;\nout vec4 stageColor;\n"
            "void main(){ gl_Position=gl_TessCoord.x*gl_in[0].gl_Position+"
            "gl_TessCoord.y*gl_in[1].gl_Position+gl_TessCoord.z*gl_in[2].gl_Position;"
            "stageColor=vec4(0.0,1.0,0.0,1.0); }\n";
        static const char *fsSource =
            "#version 460 core\nin vec4 stageColor; layout(location=0) out vec4 finalColor;\n"
            "void main(){ finalColor=stageColor; }\n";
        const GLenum types[4] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER,
                                  GL_TESS_EVALUATION_SHADER, GL_FRAGMENT_SHADER };
        const char *sources[4] = { vsSource, tcsSource, tesSource, fsSource };
        unsigned int shaders[4], program;
        unsigned char pixel[4] = {0, 0, 0, 0};
        int i, ok = 0;
        RECT client;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !getProgramInfoLog || !useProgram || !deleteProgram ||
            !drawArrays || !viewport || !readPixels) goto cleanup;
        program = createProgram();
        for (i = 0; i < 4; ++i) {
            shaders[i] = createShader(types[i]);
            shaderSource(shaders[i], 1, &sources[i], NULL);
            compileShader(shaders[i]);
            getShaderiv(shaders[i], GL_COMPILE_STATUS, &ok);
            if (!ok) { fprintf(stderr, "tessellation stage %d did not compile\n", i); goto cleanup; }
            attachShader(program, shaders[i]);
        }
        linkProgram(program);
        getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024]; int length = 0;
            getProgramInfoLog(program, sizeof(log), &length, log);
            fprintf(stderr, "tessellation program did not link: %s\n", log);
            goto cleanup;
        }
        GetClientRect(window, &client);
        viewport(0, 0, client.right, client.bottom);
        clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        clear(GL_COLOR_BUFFER_BIT);
        useProgram(program);
        drawArrays(GL_PATCHES, 0, 3);
        useProgram(0);
        readPixels(client.right / 2, client.bottom / 2, 1, 1,
                   GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        for (i = 0; i < 4; ++i) deleteShader(shaders[i]);
        deleteProgram(program);
        if (pixel[1] < 200 || pixel[0] > 32 || pixel[2] > 32) {
            fprintf(stderr, "tessellation-stage D3D9 handoff returned RGBA=(%u,%u,%u,%u)\n",
                    pixel[0], pixel[1], pixel[2], pixel[3]);
            goto cleanup;
        }
    }

    /* True transform feedback writes the requested post-VS varying bytes into
     * the application's bound buffer while rasterization is discarded. */
    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_DRAW_ARRAYS drawArrays = (PFN_DRAW_ARRAYS)getProcAddress("glDrawArrays");
        PFN_TRANSFORM_FEEDBACK_VARYINGS tfVaryings =
            (PFN_TRANSFORM_FEEDBACK_VARYINGS)getProcAddress("glTransformFeedbackVaryings");
        PFN_BEGIN_TRANSFORM_FEEDBACK beginTF =
            (PFN_BEGIN_TRANSFORM_FEEDBACK)getProcAddress("glBeginTransformFeedback");
        PFN_END_TRANSFORM_FEEDBACK endTF =
            (PFN_END_TRANSFORM_FEEDBACK)getProcAddress("glEndTransformFeedback");
        PFN_ENABLE_DISABLE enable = (PFN_ENABLE_DISABLE)required(wrapper, "glEnable");
        PFN_ENABLE_DISABLE disable = (PFN_ENABLE_DISABLE)required(wrapper, "glDisable");
        PFN_GEN_BUFFERS genBuffers = (PFN_GEN_BUFFERS)getProcAddress("glGenBuffers");
        PFN_BIND_BUFFER bindBuffer = (PFN_BIND_BUFFER)getProcAddress("glBindBuffer");
        PFN_BUFFER_DATA bufferData = (PFN_BUFFER_DATA)getProcAddress("glBufferData");
        PFN_BIND_BUFFER_BASE bindBufferBase = (PFN_BIND_BUFFER_BASE)getProcAddress("glBindBufferBase");
        PFN_GET_BUFFER_SUB_DATA getBufferSubData =
            (PFN_GET_BUFFER_SUB_DATA)getProcAddress("glGetBufferSubData");
        PFN_DELETE_BUFFERS deleteBuffers = (PFN_DELETE_BUFFERS)getProcAddress("glDeleteBuffers");
        static const char *vsSource =
            "#version 460 core\nout vec4 tfValue;\n"
            "void main(){ tfValue=vec4(1.0,2.0,3.0,4.0);"
            "gl_Position=vec4(0.0,0.0,0.0,1.0); }\n";
        static const char *fsSource =
            "#version 460 core\nlayout(location=0) out vec4 finalColor;\n"
            "void main(){ finalColor=vec4(1.0); }\n";
        const char *capture = "tfValue";
        unsigned int vs, fs, program, buffer;
        float values[12];
        int ok = 0, i;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !useProgram || !deleteProgram || !drawArrays ||
            !tfVaryings || !beginTF || !endTF || !enable || !disable ||
            !genBuffers || !bindBuffer || !bufferData || !bindBufferBase ||
            !getBufferSubData || !deleteBuffers) goto cleanup;
        vs = createShader(GL_VERTEX_SHADER);
        shaderSource(vs, 1, &vsSource, NULL); compileShader(vs);
        getShaderiv(vs, GL_COMPILE_STATUS, &ok); if (!ok) goto cleanup;
        fs = createShader(GL_FRAGMENT_SHADER);
        shaderSource(fs, 1, &fsSource, NULL); compileShader(fs);
        getShaderiv(fs, GL_COMPILE_STATUS, &ok); if (!ok) goto cleanup;
        program = createProgram(); attachShader(program, vs); attachShader(program, fs);
        tfVaryings(program, 1, &capture, GL_INTERLEAVED_ATTRIBS);
        linkProgram(program); getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) { fprintf(stderr, "transform-feedback program did not link\n"); goto cleanup; }
        memset(values, 0, sizeof(values));
        genBuffers(1, &buffer);
        bindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer);
        bufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof(values), NULL, GL_DYNAMIC_COPY);
        bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer);
        useProgram(program); enable(GL_RASTERIZER_DISCARD);
        beginTF(GL_POINTS); drawArrays(GL_POINTS, 0, 3); endTF();
        disable(GL_RASTERIZER_DISCARD); useProgram(0);
        getBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(values), values);
        for (i = 0; i < 3; ++i) {
            if (values[i * 4] != 1.0f || values[i * 4 + 1] != 2.0f ||
                values[i * 4 + 2] != 3.0f || values[i * 4 + 3] != 4.0f) {
                fprintf(stderr, "transform feedback mismatch at vertex %d: %.1f %.1f %.1f %.1f\n",
                        i, values[i * 4], values[i * 4 + 1],
                        values[i * 4 + 2], values[i * 4 + 3]);
                goto cleanup;
            }
        }
        deleteBuffers(1, &buffer); deleteShader(vs); deleteShader(fs); deleteProgram(program);
    }

    {
        PFN_CREATE_SHADER createShader = (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource = (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader = (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv = (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader = (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram = (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader = (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram = (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv = (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_USE_PROGRAM useProgram = (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram = (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_DRAW_ARRAYS_INSTANCED drawArraysInstanced =
            (PFN_DRAW_ARRAYS_INSTANCED)getProcAddress("glDrawArraysInstanced");
        PFN_VIEWPORT viewport = (PFN_VIEWPORT)required(wrapper, "glViewport");
        PFN_READ_PIXELS readPixels = (PFN_READ_PIXELS)required(wrapper, "glReadPixels");
        static const char *vsSource =
            "#version 460 core\nout vec4 instanceColor;\n"
            "const vec2 p[3]=vec2[3](vec2(-0.28,-0.45),vec2(0.28,-0.45),vec2(0.0,0.45));\n"
            "void main(){ float x=(gl_InstanceID==0)?-0.48:0.48;"
            "gl_Position=vec4(p[gl_VertexID]+vec2(x,0.0),0.0,1.0);"
            "instanceColor=(gl_InstanceID==0)?vec4(1,0,0,1):vec4(0,0,1,1); }\n";
        static const char *fsSource =
            "#version 460 core\nin vec4 instanceColor; layout(location=0) out vec4 finalColor;\n"
            "void main(){ finalColor=instanceColor; }\n";
        unsigned int vs, fs, program;
        unsigned char left[4] = {0}, right[4] = {0};
        int ok = 0;
        RECT client;

        if (!createShader || !shaderSource || !compileShader || !getShaderiv ||
            !deleteShader || !createProgram || !attachShader || !linkProgram ||
            !getProgramiv || !useProgram || !deleteProgram || !drawArraysInstanced ||
            !viewport || !readPixels) goto cleanup;
        vs = createShader(GL_VERTEX_SHADER); shaderSource(vs, 1, &vsSource, NULL);
        compileShader(vs); getShaderiv(vs, GL_COMPILE_STATUS, &ok); if (!ok) goto cleanup;
        fs = createShader(GL_FRAGMENT_SHADER); shaderSource(fs, 1, &fsSource, NULL);
        compileShader(fs); getShaderiv(fs, GL_COMPILE_STATUS, &ok); if (!ok) goto cleanup;
        program = createProgram(); attachShader(program, vs); attachShader(program, fs);
        linkProgram(program); getProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) { fprintf(stderr, "instanced GL 4.6 program did not link\n"); goto cleanup; }
        GetClientRect(window, &client); viewport(0, 0, client.right, client.bottom);
        clearColor(0, 0, 0, 1); clear(GL_COLOR_BUFFER_BIT);
        useProgram(program); drawArraysInstanced(GL_TRIANGLES, 0, 3, 2); useProgram(0);
        readPixels(client.right / 4, client.bottom / 2, 1, 1,
                   GL_RGBA, GL_UNSIGNED_BYTE, left);
        readPixels((client.right * 3) / 4, client.bottom / 2, 1, 1,
                   GL_RGBA, GL_UNSIGNED_BYTE, right);
        deleteShader(vs); deleteShader(fs); deleteProgram(program);
        if (left[0] < 200 || left[2] > 32 || right[2] < 200 || right[0] > 32) {
            fprintf(stderr, "instanced draw mismatch left=(%u,%u,%u) right=(%u,%u,%u)\n",
                    left[0], left[1], left[2], right[0], right[1], right[2]);
            goto cleanup;
        }
    }

    {
        PFN_CREATE_SHADER createShader =
            (PFN_CREATE_SHADER)getProcAddress("glCreateShader");
        PFN_SHADER_SOURCE shaderSource =
            (PFN_SHADER_SOURCE)getProcAddress("glShaderSource");
        PFN_COMPILE_SHADER compileShader =
            (PFN_COMPILE_SHADER)getProcAddress("glCompileShader");
        PFN_GET_SHADER_IV getShaderiv =
            (PFN_GET_SHADER_IV)getProcAddress("glGetShaderiv");
        PFN_DELETE_SHADER deleteShader =
            (PFN_DELETE_SHADER)getProcAddress("glDeleteShader");
        PFN_CREATE_PROGRAM createProgram =
            (PFN_CREATE_PROGRAM)getProcAddress("glCreateProgram");
        PFN_ATTACH_SHADER attachShader =
            (PFN_ATTACH_SHADER)getProcAddress("glAttachShader");
        PFN_LINK_PROGRAM linkProgram =
            (PFN_LINK_PROGRAM)getProcAddress("glLinkProgram");
        PFN_GET_PROGRAM_IV getProgramiv =
            (PFN_GET_PROGRAM_IV)getProcAddress("glGetProgramiv");
        PFN_GET_PROGRAM_INFO_LOG getProgramInfoLog =
            (PFN_GET_PROGRAM_INFO_LOG)getProcAddress("glGetProgramInfoLog");
        PFN_USE_PROGRAM useProgram =
            (PFN_USE_PROGRAM)getProcAddress("glUseProgram");
        PFN_DELETE_PROGRAM deleteProgram =
            (PFN_DELETE_PROGRAM)getProcAddress("glDeleteProgram");
        PFN_GEN_BUFFERS genBuffers =
            (PFN_GEN_BUFFERS)getProcAddress("glGenBuffers");
        PFN_BIND_BUFFER bindBuffer =
            (PFN_BIND_BUFFER)getProcAddress("glBindBuffer");
        PFN_BUFFER_DATA bufferData =
            (PFN_BUFFER_DATA)getProcAddress("glBufferData");
        PFN_BIND_BUFFER_BASE bindBufferBase =
            (PFN_BIND_BUFFER_BASE)getProcAddress("glBindBufferBase");
        PFN_GET_BUFFER_SUB_DATA getBufferSubData =
            (PFN_GET_BUFFER_SUB_DATA)getProcAddress("glGetBufferSubData");
        PFN_DELETE_BUFFERS deleteBuffers =
            (PFN_DELETE_BUFFERS)getProcAddress("glDeleteBuffers");
        PFN_DISPATCH_COMPUTE dispatchCompute =
            (PFN_DISPATCH_COMPUTE)getProcAddress("glDispatchCompute");
        PFN_MEMORY_BARRIER memoryBarrier =
            (PFN_MEMORY_BARRIER)getProcAddress("glMemoryBarrier");
        PFN_GEN_TEXTURES genTextures =
            (PFN_GEN_TEXTURES)getProcAddress("glGenTextures");
        PFN_BIND_TEXTURE bindTexture =
            (PFN_BIND_TEXTURE)getProcAddress("glBindTexture");
        PFN_TEX_IMAGE_2D texImage2D =
            (PFN_TEX_IMAGE_2D)getProcAddress("glTexImage2D");
        PFN_TEX_BUFFER_RANGE texBufferRange =
            (PFN_TEX_BUFFER_RANGE)getProcAddress("glTexBufferRange");
        PFN_BIND_IMAGE_TEXTURE bindImageTexture =
            (PFN_BIND_IMAGE_TEXTURE)getProcAddress("glBindImageTexture");
        PFN_GET_TEX_IMAGE getTexImage =
            (PFN_GET_TEX_IMAGE)getProcAddress("glGetTexImage");
        PFN_DELETE_TEXTURES deleteTextures =
            (PFN_DELETE_TEXTURES)getProcAddress("glDeleteTextures");
        static const char *computeSource =
            "#version 460 core\n"
            "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
            "layout(std430, binding=0) buffer Output { uint value; } outputData;\n"
            "layout(binding=0) uniform samplerBuffer inputTexture;\n"
            "layout(rgba8, binding=0) uniform writeonly image2D outputImage;\n"
            "void main() { outputData.value = "
            "(texelFetch(inputTexture, 0).x == 77.0) ? 0x12345678u : 0u; "
            "imageStore(outputImage, ivec2(0), vec4(0.25, 0.5, 0.75, 1.0)); }\n";
        unsigned int shader;
        unsigned int program;
        unsigned int buffer;
        unsigned int texture;
        unsigned int textureBuffer;
        unsigned int inputBuffer;
        float inputValues[5] = { -1.0f, -2.0f, -3.0f, -4.0f, 77.0f };
        unsigned int value = 0;
        unsigned char texel[4] = { 0, 0, 0, 0 };
        int compiled = 0;
        int linked = 0;

        if (!createShader || !shaderSource || !compileShader ||
            !getShaderiv || !deleteShader || !createProgram || !attachShader ||
            !linkProgram || !getProgramiv || !getProgramInfoLog || !useProgram ||
            !deleteProgram || !genBuffers || !bindBuffer || !bufferData ||
            !bindBufferBase || !getBufferSubData || !deleteBuffers ||
            !dispatchCompute || !memoryBarrier || !genTextures || !bindTexture ||
            !texImage2D || !texBufferRange || !bindImageTexture ||
            !getTexImage || !deleteTextures) {
            fprintf(stderr, "compute shader API did not resolve\n");
            goto cleanup;
        }
        shader = createShader(GL_COMPUTE_SHADER);
        shaderSource(shader, 1, &computeSource, NULL);
        compileShader(shader);
        getShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            fprintf(stderr, "GLSL 4.60 compute shader did not compile\n");
            deleteShader(shader);
            goto cleanup;
        }
        program = createProgram();
        attachShader(program, shader);
        linkProgram(program);
        getProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[1024];
            int logLength = 0;
            getProgramInfoLog(program, sizeof(log), &logLength, log);
            fprintf(stderr, "GLSL 4.60 compute program did not link: %s\n", log);
            deleteProgram(program);
            deleteShader(shader);
            goto cleanup;
        }
        genBuffers(1, &buffer);
        bindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
        bufferData(GL_SHADER_STORAGE_BUFFER, sizeof(value), &value, GL_DYNAMIC_COPY);
        bindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);
        genBuffers(1, &inputBuffer);
        bindBuffer(GL_TEXTURE_BUFFER, inputBuffer);
        bufferData(GL_TEXTURE_BUFFER, sizeof(inputValues), inputValues, GL_DYNAMIC_COPY);
        genTextures(1, &textureBuffer);
        bindTexture(GL_TEXTURE_BUFFER, textureBuffer);
        texBufferRange(GL_TEXTURE_BUFFER, GL_R32F, inputBuffer,
                       4 * sizeof(float), sizeof(float));
        genTextures(1, &texture);
        bindTexture(GL_TEXTURE_2D, texture);
        texImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, texel);
        bindImageTexture(0, texture, 0, 0, 0, GL_WRITE_ONLY, GL_RGBA8);
        useProgram(program);
        dispatchCompute(1, 1, 1);
        memoryBarrier(GL_ALL_BARRIER_BITS);
        getBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(value), &value);
        getTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
        useProgram(0);
        deleteTextures(1, &texture);
        deleteTextures(1, &textureBuffer);
        deleteBuffers(1, &inputBuffer);
        deleteBuffers(1, &buffer);
        deleteProgram(program);
        deleteShader(shader);
        if (value != 0x12345678u) {
            fprintf(stderr, "compute texture-buffer emulation returned 0x%08X instead of 0x12345678\n", value);
            goto cleanup;
        }
        if (texel[0] < 63 || texel[0] > 65 || texel[1] < 127 || texel[1] > 129 ||
            texel[2] < 190 || texel[2] > 192 || texel[3] != 255) {
            fprintf(stderr, "compute image emulation returned RGBA=(%u,%u,%u,%u)\n",
                    texel[0], texel[1], texel[2], texel[3]);
            goto cleanup;
        }
    }

    {
        PFN_GEN_LISTS genLists = (PFN_GEN_LISTS)required(wrapper, "glGenLists");
        PFN_NEW_LIST newList = (PFN_NEW_LIST)required(wrapper, "glNewList");
        PFN_END_LIST endList = (PFN_END_LIST)required(wrapper, "glEndList");
        PFN_CALL_LIST callList = (PFN_CALL_LIST)required(wrapper, "glCallList");
        PFN_DELETE_LISTS deleteLists = (PFN_DELETE_LISTS)required(wrapper, "glDeleteLists");
        PFN_COLOR_4F color4f = (PFN_COLOR_4F)required(wrapper, "glColor4f");
        PFN_GET_FLOATV getFloatv = (PFN_GET_FLOATV)required(wrapper, "glGetFloatv");
        unsigned int list;
        float before[4], compiled[4], replayed[4];
        if (!genLists || !newList || !endList || !callList || !deleteLists ||
            !color4f || !getFloatv)
            goto cleanup;
        color4f(0.9f, 0.8f, 0.7f, 0.6f);
        getFloatv(GL_CURRENT_COLOR, before);
        list = genLists(1);
        if (!list) {
            fprintf(stderr, "glGenLists failed\n");
            goto cleanup;
        }
        newList(list, GL_COMPILE);
        color4f(0.1f, 0.2f, 0.3f, 0.4f);
        endList();
        getFloatv(GL_CURRENT_COLOR, compiled);
        callList(list);
        getFloatv(GL_CURRENT_COLOR, replayed);
        deleteLists(list, 1);
        if (memcmp(before, compiled, sizeof(before)) != 0 ||
            replayed[0] != 0.1f || replayed[1] != 0.2f ||
            replayed[2] != 0.3f || replayed[3] != 0.4f) {
            fprintf(stderr, "display-list compile/replay state mismatch: "
                    "before=(%g,%g,%g,%g) compiled=(%g,%g,%g,%g) "
                    "replayed=(%g,%g,%g,%g)\n",
                    before[0], before[1], before[2], before[3],
                    compiled[0], compiled[1], compiled[2], compiled[3],
                    replayed[0], replayed[1], replayed[2], replayed[3]);
            goto cleanup;
        }
    }

    clearColor(0.125f, 0.25f, 0.5f, 1.0f);
    clear(GL_COLOR_BUFFER_BIT);
    if (!swapBuffers(dc)) {
        fprintf(stderr, "SwapBuffers failed: %lu\n", (unsigned long)GetLastError());
        goto cleanup;
    }

    printf("GL_VERSION=%s\n", (const char *)version);
    result = 0;

cleanup:
    if (makeCurrent)
        makeCurrent(NULL, NULL);
    if (context && deleteContext)
        deleteContext(context);
    if (dc && window)
        ReleaseDC(window, dc);
    if (window)
        DestroyWindow(window);
    if (windowClass)
        UnregisterClassA("GLDirectWglSmoke", instance);
    FreeLibrary(wrapper);
    return result;
}
