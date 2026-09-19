IM_MSVC_RUNTIME_CHECKS_OFF
REFLECT()
struct Exported {
    PROPERTY() ALIGNED(16) float matrix[16];
    METHOD() IMGUI_API void Draw();
};
