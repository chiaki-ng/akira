#ifndef AKIRA_DEKO3D_RENDERER_HPP
#define AKIRA_DEKO3D_RENDERER_HPP

#ifdef BOREALIS_USE_DEKO3D

#include "stream/video_renderer.hpp"

#include <deko3d.hpp>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <borealis.hpp>
#include <borealis/platforms/switch/switch_video.hpp>
#include <nanovg/dk_renderer.hpp>
#include <nanovg/framework/CShader.h>
#include <nanovg/framework/CMemPool.h>
#include <nanovg/framework/CCmdMemRing.h>
#include <nanovg/framework/CDescriptorSet.h>

extern "C"
{
#include <libavutil/hwcontext_nvtegra.h>
}

class Deko3dRenderer : public IVideoRenderer
{
public:
    Deko3dRenderer();
    ~Deko3dRenderer() override;

    bool initialize(int frame_width, int frame_height) override;
    bool isInitialized() const override { return m_initialized; }
    void draw(AVFrame* frame) override;
    void presentFrame(AVFrame* frame) override;
    void cleanup() override;
    void waitIdle() override;

    void setStatsOverlayMode(StatsOverlayMode mode) override
    {
        m_stats_mode.store(mode, std::memory_order_relaxed);
        if (mode == StatsOverlayMode::Off)
            m_ovl_front.store(-1, std::memory_order_release);
        else
            m_ovl_dirty = true;
    }
    bool overlayTouchBegin(float x, float y) override;
    void overlayTouchMove(float x, float y) override;
    void overlayTouchEnd() override;
    bool takeOverlayPositionDirty() override;
    void setStreamStats(const StreamStats& stats) override { m_stats = stats; }
    float getRenderFPS() const override { return m_render_fps; }
    void setPaused(bool paused) { m_paused.store(paused, std::memory_order_relaxed); }
    void setConnectionContext(const std::string& context) override;
    void updateResolution(int width, int height) { m_frame_width = width; m_frame_height = height; }

    void triggerBorderFlash() {
        m_border_flash_frames.store(BORDER_FLASH_DURATION, std::memory_order_release);
    }

    bool captureLastFrame(std::vector<uint8_t>& rgba, int& width, int& height) override;

    void setTickCallback(TickCallback cb) override { m_tick_callback = std::move(cb); }

    void setDithering(bool enabled);

private:
    std::atomic<bool> m_paused{false};
    std::optional<CMemPool> m_pool_code;
    std::optional<CMemPool> m_pool_data;

    void renderBorderFlash();

    enum class Tone { Good = 0, Warn = 1, Bad = 2 };

    struct OverlayText
    {
        std::string fps, rate, loss, rtt, uptime;
        std::string req_res, req_fps, req_rate, req_codec;
        std::string out_res, out_codec, out_path;
        std::string fsr_path, fsr_detail, fsr_compact;
        bool fsr_active = false;
        std::string lost, recovered;
        std::string ghash, vpn, context;
        std::string lat_net, lat_visual, lat_total, lat_jitter, lat_decode, src_fps;
        float lat_net_ms = 0.0f, lat_visual_ms = 0.0f, lat_total_ms = 0.0f;
        bool lat_valid = false;
    };

    OverlayText m_ov;
    std::string m_connection_context;
    std::chrono::steady_clock::time_point m_ov_last_rebuild{};
    int m_overlay_font = -1;
    float m_overlay_scale = 1.0f;
    bool m_font_atlas_warm = false;

    void warmFontAtlas(NVGcontext* vg);
    void rebuildOverlayText();
    bool ensureOverlayShaders();
    bool ensureOverlayContext();
    void destroyOverlayContext();
    bool ensureOverlayTarget();
    void destroyOverlayTarget();
    void compositeOverlay();

public:
    void updateOverlayTexture() override;

private:
    Tone toneFps() const;
    Tone toneLoss() const;
    Tone toneRate() const;
    Tone toneRtt() const;
    Tone toneLost() const;
    Tone toneLatency() const;
    NVGcolor toneColor(Tone tone) const;
    void drawOverlayPanel(NVGcontext* vg, float x, float y, float w, float h);
    void drawStatRow(NVGcontext* vg, float x, float y, float w,
                     const char* label, const std::string& value,
                     const char* unit, Tone tone);
    void drawFsrRow(NVGcontext* vg, float x, float y, float w);
    void drawCompactCell(NVGcontext* vg, float& cursorX, float centerY,
                         const char* label, const std::string& value,
                         const char* unit, Tone tone, bool first);
    void drawCompactFsrLine(NVGcontext* vg, float x, float centerY);
    void drawLatencyStrip(NVGcontext* vg, float x, float y, float w, float h);
    void drawCompactOverlay(NVGcontext* vg);
    void drawFullOverlay(NVGcontext* vg);

    std::atomic<int> m_border_flash_frames{0};
    static constexpr int BORDER_FLASH_DURATION = 20;




    bool m_initialized = false;

    int m_frame_width = 0;
    int m_frame_height = 0;

    brls::SwitchVideoContext* m_vctx = nullptr;
    bool m_overlay_to_texture = false;
    bool m_ovl_dirty = true;

    dk::UniqueQueue m_ovl_queue;
    std::optional<CMemPool> m_ovl_images_pool;
    std::optional<CMemPool> m_ovl_code_pool;
    std::optional<CMemPool> m_ovl_data_pool;
    std::optional<nvg::DkRenderer> m_ovl_renderer;
    NVGcontext* m_ovl_vg = nullptr;
    bool m_ovl_ctx_ready = false;

    dk::UniqueCmdBuf m_ovl_rt_cmdbuf;
    std::optional<CCmdMemRing<2>> m_ovl_rt_cmdmem;

    dk::ImageLayout m_ovl_rt_layout;
    dk::ImageLayout m_ovl_ds_layout;
    dk::Image m_ovl_rt_image[2];
    dk::Image m_ovl_ds_image;
    dk::ImageDescriptor m_ovl_rt_desc[2];
    CMemPool::Handle m_ovl_rt_handle[2];
    CMemPool::Handle m_ovl_ds_handle;
    int m_ovl_rt_texture_id[2] = { 0, 0 };
    std::atomic<int> m_ovl_front{-1};
    dk::UniqueCmdBuf m_ovl_desc_cmdbuf;
    CMemPool::Handle m_ovl_desc_cmdmem;
    DkCmdList m_ovl_desc_list = 0;
    std::atomic<bool> m_ovl_desc_pending{false};
    bool m_ovl_desc_live = false;
    unsigned m_ovl_rt_w = 0;
    unsigned m_ovl_rt_h = 0;
    float m_ovl_rt_scale = 0.0f;

    CShader m_ovl_vertex_shader;
    CShader m_ovl_fragment_shader;
    CShader m_border_fragment_shader;
    bool m_ovl_shaders_ready = false;

    dk::UniqueCmdBuf m_ovl_cmdbuf;
    std::optional<CCmdMemRing<2>> m_ovl_cmdmem;
    CMemPool::Handle m_ovl_uniform;
    unsigned m_ovl_uniform_slot = 0;
    unsigned m_border_uniform_slot = 0;

    std::atomic<float> m_ovl_panel_w{0.0f};
    std::atomic<float> m_ovl_panel_h{0.0f};
    dk::Device m_device;
    dk::Queue m_queue;

    dk::UniqueCmdBuf m_cmdbuf;
    DkCmdList m_video_cmdlist = 0;

    dk::UniqueCmdBuf m_update_cmdbuf;
    CMemPool::Handle m_update_cmdmem;
    uint32_t m_update_cmdmem_slice = 0;
    static constexpr unsigned UpdateCmdSliceSize = 0x1000;

    dk::UniqueCmdBuf m_overlay_cmdbuf;
    static constexpr unsigned OverlayCmdSlices = 4;
    static constexpr unsigned OverlayCmdSliceSize = 0x2000;
    std::optional<CCmdMemRing<OverlayCmdSlices>> m_overlay_cmdmem;

    std::atomic<float> m_overlay_norm_x{-1.0f};
    std::atomic<float> m_overlay_norm_y{-1.0f};
    std::atomic<float> m_overlay_x{0.0f};
    std::atomic<float> m_overlay_y{0.0f};
    std::atomic<float> m_overlay_w{0.0f};
    std::atomic<float> m_overlay_h{0.0f};
    bool m_overlay_drag = false;
    std::mutex m_overlay_touch_mutex;
    std::atomic<float> m_overlay_hit_scale{1.0f};
    std::atomic<bool> m_overlay_pos_dirty{false};
    float m_overlay_grab_dx = 0.0f;
    float m_overlay_grab_dy = 0.0f;
    void overlayOrigin(float w, float h, float& ox, float& oy);
    void updateOverlayPlacement();

    CShader m_vertex_shader;

    struct ShaderVariantKey {
        bool dithering = false;
        bool fsrEasu = false;
        bool fsrRcas = false;
        bool fsrPass = false;

        bool operator==(const ShaderVariantKey& other) const = default;
    };

    struct CachedFragmentShader {
        ShaderVariantKey key;
        CShader shader;
    };

    CShader* m_fragment_shader = nullptr;

    CMemPool::Handle m_vertex_buffer;

    dk::ImageLayout m_luma_layout;
    dk::ImageLayout m_chroma_layout;

    struct FrameMapping {
        uint32_t handle = 0;
        void* cpuAddr = nullptr;
        uint32_t size = 0;
        uint32_t chromaOffset = 0;
        dk::UniqueMemBlock memblock;
        dk::Image luma;
        dk::Image chroma;
        dk::ImageDescriptor lumaDesc;
        dk::ImageDescriptor chromaDesc;
    };

    std::vector<FrameMapping> m_frame_mappings;
    int m_current_mapping_index = -1;

    int m_luma_texture_id = 0;
    int m_chroma_texture_id = 0;

    bool m_textures_initialized = false;

    bool setupTextures(AVFrame* frame);
    void releaseTextures();
    void updateFrameBindings(AVFrame* frame);
    void recordStaticVideoCommands();

    bool compileShaderFromSource(CShader& shader, const std::string& source, bool isVertex);
    bool ensureVertexShaderCompiled();
    CShader* getOrCreateFragmentShader(const ShaderVariantKey& key);
    bool compileVideoShaders(bool dithering);
    bool m_dithering_enabled = false;
    bool m_uam_initialized = false;
    bool m_fsr_enabled = false;
    bool m_fsr_pending = false;
    bool m_easu_enabled = false;
    bool m_rcas_enabled = false;
    bool m_fsr_supersampling = false;
    float m_fsr_sharpness = 0.2f;
    int m_display_width = 0;
    int m_display_height = 0;
    int m_fsr_target_width = 0;
    int m_fsr_target_height = 0;

    CShader* m_fsr_easu_shader = nullptr;
    CShader* m_fsr_rcas_shader = nullptr;
    CShader* m_fsr_pass_shader = nullptr;
    std::vector<std::unique_ptr<CachedFragmentShader>> m_fragment_shader_cache;

    CMemPool::Handle m_rt_easu_handle;
    dk::Image m_rt_easu_image;
    dk::ImageLayout m_rt_easu_layout;
    dk::ImageDescriptor m_rt_easu_desc;
    int m_rt_easu_texture_id = 0;

    CMemPool::Handle m_rt_rcas_handle;
    dk::Image m_rt_rcas_image;
    dk::ImageDescriptor m_rt_rcas_desc;
    int m_rt_rcas_texture_id = 0;

    dk::UniqueCmdBuf m_fsr_static_cmdbuf;
    CMemPool::Handle m_fsr_static_cmdmem;
    static constexpr unsigned FsrStaticCmdSize = 0x10000;
    DkCmdList m_fsr_easu_cmdlist = 0;

    dk::UniqueCmdBuf m_fsr_rcas_cmdbuf;
    CMemPool::Handle m_fsr_rcas_cmdmem;
    static constexpr unsigned FsrRcasCmdSize = 0x4000;

    CMemPool::Handle m_fsr_uniform_buffer;

    void initFsr();
    void cleanupFsr();
    void recordFsrCommands();
    void computeFsrConstants();

    bool m_frame_bound = false;

    static constexpr int CAPTURE_WIDTH = 640;
    static constexpr int HW_TRANSFER_ALIGNMENT = 256;
    static constexpr int FRAME_RING_SIZE = 3;
    std::mutex m_frame_ring_mutex;
    AVFrame* m_frame_ring[FRAME_RING_SIZE] = {};
    int m_frame_ring_index = 0;
    AVFrame* m_current_frame = nullptr;

    void renderVideo();
    void recordPresentedFrame();

    uint64_t m_render_frame_count = 0;
    std::chrono::steady_clock::time_point m_render_fps_start;
    float m_render_fps = 0.0f;
    bool m_render_fps_init = false;

    void registerCallback();
    void unregisterCallback();
    bool m_callback_registered = false;
    TickCallback m_tick_callback;
};

#endif // BOREALIS_USE_DEKO3D

#endif // AKIRA_DEKO3D_RENDERER_HPP
