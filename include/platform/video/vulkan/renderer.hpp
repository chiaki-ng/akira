#ifndef AKIRA_VULKAN_RENDERER_HPP
#define AKIRA_VULKAN_RENDERER_HPP

#include "platform/video/viddec/hardware_decoder.hpp"
#include "platform/video/viddec/output_memory.hpp"
#include "platform/video/renderer.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <borealis.hpp>
#include <borealis/core/vk_handle.hpp>
#include <borealis/platforms/switch/switch_video_vk.hpp>
#include <nanovg/vk_renderer.hpp>

namespace akira::platform::vulkan
{

class VulkanRenderer final : public VideoRenderer
{
public:
    VulkanRenderer();
    ~VulkanRenderer() override;
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    bool initialize(int frame_width, int frame_height) override;
    bool isInitialized() const override { return m_initialized.load(std::memory_order_acquire); }
    using Frame = akira::platform::viddec::HardwareDecoder::Frame;

    void draw(Frame frame);
    void presentFrame(Frame frame);
    void cleanup() override;
    void waitIdle() override;

    void updateOverlayTexture() override;
    void setStatsOverlayMode(StatsOverlayMode mode) override;
    bool overlayTouchBegin(float x, float y) override;
    void overlayTouchMove(float x, float y) override;
    void overlayTouchEnd() override;
    bool takeOverlayPositionDirty() override;
    float getRenderFPS() const override { return m_render_fps.load(std::memory_order_relaxed); }
    void setPaused(bool paused) override { m_paused.store(paused, std::memory_order_relaxed); }
    void setConnectionContext(const std::string& context) override;
    void triggerBorderFlash() override
    {
        m_border_flash_frames.store(BORDER_FLASH_DURATION, std::memory_order_release);
    }

    bool captureLastFrame(std::vector<uint8_t>& rgba, int& width, int& height) override;
    std::shared_ptr<akira::platform::viddec::OutputMemory> decoderOutputMemory() const;

    void setTickCallback(TickCallback cb) override { m_tick_callback = std::move(cb); }

    void setDithering(bool enabled);

private:

    using DeviceMemory = brls::vk::DeviceHandle<VkDeviceMemory, &vkFreeMemory>;
    using Image = brls::vk::DeviceHandle<VkImage, &vkDestroyImage>;
    using ImageView = brls::vk::DeviceHandle<VkImageView, &vkDestroyImageView>;
    using Buffer = brls::vk::DeviceHandle<VkBuffer, &vkDestroyBuffer>;
    using BufferView = brls::vk::DeviceHandle<VkBufferView, &vkDestroyBufferView>;
    using Sampler = brls::vk::DeviceHandle<VkSampler, &vkDestroySampler>;
    using ShaderModule = brls::vk::DeviceHandle<VkShaderModule, &vkDestroyShaderModule>;
    using DescriptorSetLayout = brls::vk::DeviceHandle<VkDescriptorSetLayout, &vkDestroyDescriptorSetLayout>;
    using DescriptorPool = brls::vk::DeviceHandle<VkDescriptorPool, &vkDestroyDescriptorPool>;
    using PipelineLayout = brls::vk::DeviceHandle<VkPipelineLayout, &vkDestroyPipelineLayout>;
    using Pipeline = brls::vk::DeviceHandle<VkPipeline, &vkDestroyPipeline>;
    using CommandPool = brls::vk::DeviceHandle<VkCommandPool, &vkDestroyCommandPool>;
    using Fence = brls::vk::DeviceHandle<VkFence, &vkDestroyFence>;

    enum class PlanePath { Image = 0, TexelBuffer = 1 };
    enum class DecoderSampling { Unchecked, Verified, Broken };
    enum class Program { Video, Easu, Rcas, Pass, Overlay, Border };
    enum class Target { Swapchain, Offscreen };
    enum class Shader
    {
        FullscreenVertex,
        OverlayVertex,
        VideoImage,
        VideoTexel,
        EasuImage,
        EasuTexel,
        Rcas,
        Pass,
        Overlay,
        Border,
    };
    static constexpr std::size_t ShaderCount = 10;

    struct PipelineKey
    {
        Program program = Program::Video;
        Target target = Target::Swapchain;
        PlanePath path = PlanePath::Image;
        bool dithering = false;

        auto operator<=>(const PipelineKey&) const = default;
    };

    struct VideoPush
    {
        std::array<std::uint32_t, 16> easu{};
        std::array<std::uint32_t, 4> planes{};
        std::array<std::uint32_t, 4> size{};
    };

    struct QuadPush
    {
        std::array<float, 4> first{};
        std::array<float, 4> second{};
    };

    struct PlaneLayout
    {
        const void* backing = nullptr;
        std::size_t backingSize = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        akira::platform::viddec::HardwarePlane luma{};
        akira::platform::viddec::HardwarePlane chroma{};
        VkBuffer decoderBuffer = VK_NULL_HANDLE;

        std::uint32_t chromaWidth() const { return (width + 1) / 2; }
        std::uint32_t chromaHeight() const { return (height + 1) / 2; }
        std::size_t lumaEnd() const { return luma.offset + std::size_t{luma.pitch} * (height - 1) + width; }
        std::size_t chromaEnd() const
        {
            return chroma.offset + std::size_t{chroma.pitch} * (chromaHeight() - 1) + std::size_t{chromaWidth()} * 2;
        }
        bool operator==(const PlaneLayout& other) const
        {
            return backing == other.backing && backingSize == other.backingSize && width == other.width &&
                height == other.height && luma.offset == other.luma.offset && luma.pitch == other.luma.pitch &&
                chroma.offset == other.chroma.offset && chroma.pitch == other.chroma.pitch &&
                decoderBuffer == other.decoderBuffer;
        }
    };

    struct Capabilities
    {
        VkDeviceSize hostPointerAlignment = 0;
        bool imagePlanes = false;
        bool texelPlanes = false;
        VkExtent3D imageMaxExtent{};
        std::uint32_t maxTexelBufferElements = 0;
        VkDeviceSize texelOffsetAlignment = 1;
    };

    struct HostRange
    {
        std::uint32_t typeBits = 0;
        VkDeviceSize size = 0;
    };

    struct ImagePlanes
    {
        DeviceMemory memory;
        Image luma;
        Image chroma;
        ImageView lumaView;
        ImageView chromaView;
    };

    struct TexelPlanes
    {
        DeviceMemory memory;
        Buffer buffer;
        BufferView luma;
        BufferView chroma;
        std::array<std::uint32_t, 4> bases{};
        void* mapped = nullptr;
        std::size_t size = 0;
    };

    class PlaneImport
    {
    public:
        PlaneImport(PlaneLayout layout, std::variant<ImagePlanes, TexelPlanes> planes, VkDescriptorSet set);

        const PlaneLayout& layout() const { return m_layout; }
        VkDescriptorSet descriptorSet() const { return m_set; }
        const std::array<std::uint32_t, 4>& bases() const { return m_bases; }
        void appendAcquireBarriers(std::vector<VkImageMemoryBarrier2>& barriers);
        void copyFrom(const void* backing);

    private:
        PlaneLayout m_layout;
        std::variant<ImagePlanes, TexelPlanes> m_planes;
        VkDescriptorSet m_set;
        std::array<std::uint32_t, 4> m_bases;
        bool m_needs_layout = true;
    };

    class ImportCache
    {
    public:
        ImportCache(std::uint64_t generation, PlanePath path, DescriptorPool pool);

        std::uint64_t generation() const { return m_generation; }
        PlanePath path() const { return m_path; }
        VkDescriptorPool pool() const { return m_pool.get(); }
        PlaneImport* find(std::uint32_t slot);
        PlaneImport& store(std::uint32_t slot, std::unique_ptr<PlaneImport> import);
        bool failed(std::uint32_t slot) const;
        void markFailed(std::uint32_t slot);

    private:
        std::uint64_t m_generation;
        PlanePath m_path;
        DescriptorPool m_pool;
        std::array<std::unique_ptr<PlaneImport>, akira::platform::viddec::HardwareDecoder::ImageCount> m_slots;
        std::array<bool, akira::platform::viddec::HardwareDecoder::ImageCount> m_failed{};
    };

    class RenderTarget
    {
    public:
        RenderTarget(DeviceMemory memory, Image image, ImageView view, VkExtent2D extent, VkDescriptorSet set);

        VkImage image() const { return m_image.get(); }
        VkImageView view() const { return m_view.get(); }
        VkExtent2D extent() const { return m_extent; }
        VkDescriptorSet descriptorSet() const { return m_set; }

    private:
        DeviceMemory m_memory;
        Image m_image;
        ImageView m_view;
        VkExtent2D m_extent;
        VkDescriptorSet m_set;
    };

    struct FsrConfig
    {
        bool easu = false;
        bool rcas = false;
        bool supersampling = false;
        bool rcasTarget = false;
        bool dithering = false;
        float sharpness = 0.2f;
        int inputWidth = 0;
        int inputHeight = 0;
        int targetWidth = 0;
        int targetHeight = 0;
        VkExtent2D display{};
        std::array<std::uint32_t, 16> easuConstants{};
        std::array<std::uint32_t, 4> rcasConstants{};
    };

    struct FsrSummary
    {
        bool active = false;
        bool easu = false;
        bool rcas = false;
        bool supersampling = false;
        float sharpness = 0.2f;
        int inputHeight = 0;
        int targetHeight = 0;
        int displayHeight = 0;
    };

    class FsrState
    {
    public:
        FsrState(FsrConfig config, DescriptorPool pool, RenderTarget easu, std::optional<RenderTarget> rcas);

        const FsrConfig& config() const { return m_config; }
        const RenderTarget& easuTarget() const { return m_easu; }
        const RenderTarget* rcasTarget() const { return m_rcas ? &*m_rcas : nullptr; }

    private:
        FsrConfig m_config;
        DescriptorPool m_pool;
        RenderTarget m_easu;
        std::optional<RenderTarget> m_rcas;
    };

    class OverlayTarget
    {
    public:
        OverlayTarget(DescriptorPool pool, std::array<RenderTarget, 2> colors, RenderTarget stencil);

        const RenderTarget& color(int index) const { return m_colors[static_cast<std::size_t>(index)]; }
        const RenderTarget& stencil() const { return m_stencil; }
        VkExtent2D extent() const { return m_colors[0].extent(); }

    private:
        DescriptorPool m_pool;
        std::array<RenderTarget, 2> m_colors;
        RenderTarget m_stencil;
    };

    class OverlayContext
    {
    public:
        OverlayContext(CommandPool pool, std::array<VkCommandBuffer, 2> commands, Fence fence,
            std::unique_ptr<nvg::VkRenderer> renderer, NVGcontext* vg, int font);
        ~OverlayContext();
        OverlayContext(const OverlayContext&) = delete;
        OverlayContext& operator=(const OverlayContext&) = delete;

        VkCommandPool pool() const { return m_pool.get(); }
        VkCommandBuffer upload() const { return m_commands[0]; }
        VkCommandBuffer draw() const { return m_commands[1]; }
        VkFence fence() const { return m_fence.get(); }
        nvg::VkRenderer& renderer() { return *m_renderer; }
        NVGcontext* vg() const { return m_vg; }
        int font() const { return m_font; }

    private:
        CommandPool m_pool;
        std::array<VkCommandBuffer, 2> m_commands;
        Fence m_fence;
        std::unique_ptr<nvg::VkRenderer> m_renderer;
        NVGcontext* m_vg;
        int m_font;
    };

    struct SharedObjects
    {
        std::array<ShaderModule, ShaderCount> shaders;
        Sampler sampler;
        DescriptorSetLayout imageSetLayout;
        DescriptorSetLayout texelSetLayout;
        DescriptorSetLayout textureSetLayout;
        PipelineLayout imagePipelineLayout;
        PipelineLayout texelPipelineLayout;
        PipelineLayout texturePipelineLayout;
        std::map<PipelineKey, Pipeline> pipelines;
    };

    struct Retired
    {
        Frame frame;
        std::shared_ptr<ImportCache> cache;
        std::shared_ptr<FsrState> fsr;
        std::shared_ptr<OverlayTarget> overlay;
    };

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

    static constexpr int BORDER_FLASH_DURATION = 20;
    static constexpr int CAPTURE_WIDTH = 640;
    static constexpr VkFormat OffscreenFormat = VK_FORMAT_R8G8B8A8_UNORM;
    static constexpr std::uint64_t LinearModifier = 0;
    static constexpr std::uint32_t SamplingProbeRows = 16;
    static constexpr std::uint64_t SamplingProbeTimeoutNs = 1'000'000'000;

    static bool succeeded(VkResult result, std::string_view what);
    static float ovlSize(float px);
    static NVGcolor ovl(NVGcolor color, unsigned char alpha);
    static VkImageAspectFlags stencilAspect(VkFormat format);
    static void imageBarrier(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, VkImageLayout oldLayout,
        VkImageLayout newLayout, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
        VkAccessFlags2 dstAccess);
    static void setViewport(VkCommandBuffer cmd, VkExtent2D extent);
    static FsrConfig fsrConfig(int inputWidth, int inputHeight, VkExtent2D display);

    static VkShaderStageFlags pushStages(Program program);

    bool queryCapabilities();
    std::optional<VkExtent3D> queryImagePlanes() const;
    bool queryTexelPlanes() const;
    bool createSharedObjects();
    std::optional<ShaderModule> createShaderModule(std::span<const std::uint32_t> code) const;
    std::optional<DescriptorSetLayout> createSetLayout(VkDescriptorType type, std::uint32_t bindings) const;
    std::optional<PipelineLayout> createPipelineLayout(VkDescriptorSetLayout setLayout, VkShaderStageFlags stages,
        std::uint32_t pushSize) const;
    std::optional<DescriptorPool> createDescriptorPool(VkDescriptorType type, std::uint32_t sets,
        std::uint32_t descriptors) const;
    std::optional<VkDescriptorSet> allocateSet(VkDescriptorPool pool, VkDescriptorSetLayout layout) const;
    std::optional<RenderTarget> createRenderTarget(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage,
        VkImageAspectFlags aspect, VkDescriptorPool pool) const;
    std::optional<HostRange> hostRange(const PlaneLayout& layout) const;
    std::optional<DeviceMemory> importHostMemory(const PlaneLayout& layout, std::uint32_t typeBits,
        VkDeviceSize size) const;
    std::optional<ImagePlanes> importImagePlanes(const PlaneLayout& layout, const HostRange& range) const;
    std::optional<TexelPlanes> stagingTexelPlanes(const PlaneLayout& layout, const HostRange& range) const;
    std::optional<TexelPlanes> texelViews(const PlaneLayout& layout, VkBuffer buffer) const;
    std::unique_ptr<PlaneImport> importPlanes(const PlaneLayout& layout, const HostRange& range,
        ImportCache& cache) const;
    std::shared_ptr<ImportCache> createImportCache(std::uint64_t generation) const;
    PlaneLayout planeLayout(const akira::platform::viddec::HardwareFrame& frame) const;
    bool decoderSamplingMatches(const PlaneLayout& layout);
    PlaneImport* acquireImport(const Frame& frame);
    VkPipelineLayout layoutFor(const PipelineKey& key) const;
    VkPipeline pipeline(const PipelineKey& key);
    std::optional<Pipeline> createPipeline(const PipelineKey& key) const;
    std::shared_ptr<FsrState> createFsr(const FsrConfig& config) const;
    void publishFsrSummary(const FsrState* fsr);
    void retire(Retired retired);

    void renderVideo();
    void recordAcquire(PlaneImport& import);
    void drawVideoFsr(VkCommandBuffer cmd, const PlaneImport& import, const FsrState& fsr, VideoPush push);
    void drawPass(VkCommandBuffer cmd, const PipelineKey& key, VkExtent2D extent, VkDescriptorSet set,
        std::span<const std::byte> push);
    void renderOffscreen(VkCommandBuffer cmd, const RenderTarget& target, const PipelineKey& key, VkDescriptorSet set,
        std::span<const std::byte> push);
    bool ensureSwapchainRendering();
    void recordPresentedFrame();
    std::shared_ptr<OverlayTarget> compositeOverlay(VkCommandBuffer cmd);
    void renderBorderFlash(VkCommandBuffer cmd);

    void registerCallback();
    void unregisterCallback();

    bool ensureOverlayContext();
    void destroyOverlayContext();
    bool settleOverlay(bool wait);
    bool ensureOverlayTarget();
    bool renderOverlay(const OverlayTarget& target, int back);
    void warmFontAtlas(NVGcontext* vg);
    void rebuildOverlayText();

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
    void overlayOrigin(float w, float h, float& ox, float& oy);
    void updateOverlayPlacement();

    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_dithering_enabled{false};

    brls::SwitchVideoContext* m_vctx = nullptr;
    VkDevice m_device = VK_NULL_HANDLE;
    PFN_vkGetMemoryHostPointerPropertiesEXT m_get_host_pointer_properties = nullptr;
    Capabilities m_caps;
    PlanePath m_path = PlanePath::Image;
    DecoderSampling m_decoder_sampling = DecoderSampling::Unchecked;

    SharedObjects m_shared;

    std::mutex m_render_mutex;
    std::shared_ptr<ImportCache> m_cache;
    std::shared_ptr<FsrState> m_fsr;
    bool m_fsr_pending = false;
    int m_configured_width = 0;
    int m_configured_height = 0;

    std::mutex m_fsr_summary_mutex;
    FsrSummary m_fsr_summary;

    std::mutex m_frame_mutex;
    Frame m_current_frame;
    Frame m_last_frame;
    std::atomic<bool> m_frame_bound{false};

    std::uint64_t m_render_frame_count = 0;
    std::chrono::steady_clock::time_point m_render_fps_start;
    std::atomic<float> m_render_fps{0.0f};
    std::int64_t m_drawn_timestamp = -1;
    std::uint32_t m_drawn_slot = 0;
    bool m_render_fps_init = false;

    std::atomic<int> m_border_flash_frames{0};

    OverlayText m_ov;
    std::string m_connection_context;
    std::chrono::steady_clock::time_point m_ov_last_rebuild{};
    float m_overlay_scale = 1.0f;
    bool m_font_atlas_warm = false;
    bool m_overlay_to_texture = false;
    bool m_ovl_dirty = true;

    std::unique_ptr<OverlayContext> m_ovl_context;
    std::mutex m_ovl_mutex;
    std::shared_ptr<OverlayTarget> m_ovl_target;
    int m_ovl_front = -1;
    struct OverlayInFlight
    {
        std::shared_ptr<OverlayTarget> target;
        int back = 0;
        float width = 0.0f;
        float height = 0.0f;
    };
    std::optional<OverlayInFlight> m_ovl_in_flight;
    float m_ovl_panel_w = 0.0f;
    float m_ovl_panel_h = 0.0f;

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

    bool m_callback_registered = false;
    TickCallback m_tick_callback;
};


}

#endif
