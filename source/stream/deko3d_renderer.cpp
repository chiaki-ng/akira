#ifdef BOREALIS_USE_DEKO3D

#include "stream/deko3d_renderer.hpp"

#include <borealis/core/assets.hpp>
#include <nanovg_dk.h>
#include "core/wireguard_manager.hpp"
#include "core/settings_manager.hpp"
#include "ui/theme.hpp"
#include <borealis.hpp>
#include <borealis/platforms/switch/switch_platform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <format>
#include <uam.h>

extern "C"
{
#include <libavutil/pixfmt.h>
#include <libavutil/hwcontext_nvtegra.h>
#include <libswscale/swscale.h>
}

namespace
{
    float ovlSize(float px)
    {
        if (px > 1.0f && px < 200.0f)
            return px;
        return px <= 1.0f ? 8.0f : 24.0f;
    }

    static constexpr unsigned StaticCmdSize = 0x10000;

    struct Vertex
    {
        float position[3];
        float uv[2];
    };

    // Text vertex with color

    constexpr std::array VertexAttribState =
    {
        DkVtxAttribState{ 0, 0, offsetof(Vertex, position), DkVtxAttribSize_3x32, DkVtxAttribType_Float, 0 },
        DkVtxAttribState{ 0, 0, offsetof(Vertex, uv), DkVtxAttribSize_2x32, DkVtxAttribType_Float, 0 },
    };

    constexpr std::array VertexBufferState =
    {
        DkVtxBufferState{ sizeof(Vertex), 0 },
    };

    // Text vertex attributes


    // Full-screen quad vertices (NDC coordinates)
    constexpr std::array QuadVertexData =
    {
        Vertex{ { -1.0f, +1.0f, 0.0f }, { 0.0f, 0.0f } },
        Vertex{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
        Vertex{ { +1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
        Vertex{ { +1.0f, +1.0f, 0.0f }, { 1.0f, 0.0f } },
    };

    // Helper to convert pixel coords to NDC
    inline void pixelToNDC(float px, float py, int screenW, int screenH, float& ndcX, float& ndcY)
    {
        ndcX = (px / screenW) * 2.0f - 1.0f;
        ndcY = 1.0f - (py / screenH) * 2.0f;
    }
}

static std::string loadShaderSource(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        brls::Logger::error("Failed to open shader source: {}", path);
        return "";
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    std::string source(size, '\0');
    fread(&source[0], 1, size, f);
    fclose(f);
    return source;
}

void Deko3dRenderer::setConnectionContext(const std::string& context)
{
    m_connection_context = context;
    m_ovl_dirty = true;
}

static std::string injectShaderDefines(const std::string& source, const std::string& defines)
{
    if (defines.empty())
        return source;

    // Find #version directive if it exists
    size_t version_pos = source.find("#version");
    if (version_pos == std::string::npos)
    {
        // No #version, insert at first newline
        auto pos = source.find('\n');
        if (pos == std::string::npos)
            return source + "\n" + defines;
        std::string result = source;
        result.insert(pos + 1, defines);
        return result;
    }

    // Found #version, insert after the #version line
    size_t insert_pos = source.find('\n', version_pos);
    if (insert_pos == std::string::npos)
        return source + "\n" + defines;

    std::string result = source;
    result.insert(insert_pos + 1, defines);
    return result;
}

static std::string buildVideoFragmentDefines(bool dithering, bool fsrEasu, bool fsrRcas, bool fsrPass)
{
    std::string defines;

    if (fsrEasu)
        defines += "#define FSR_EASU\n";
    if (fsrRcas)
        defines += "#define FSR_RCAS\n";
    if (fsrPass)
        defines += "#define FSR_PASS\n";
    if (dithering)
    {
        float strength = SettingsManager::getInstance()->getDitheringStrength();
        defines += std::format("#define DITHER_NOISE\n#define DITHER_STRENGTH {:.1f}\n", strength);
    }

    return defines;
}

Deko3dRenderer::Deko3dRenderer()
{
    uam_init();
    m_uam_initialized = true;
}

Deko3dRenderer::~Deko3dRenderer()
{
    cleanup();

    m_vertex_shader.destroy();
    for (auto& variant : m_fragment_shader_cache)
        variant->shader.destroy();
    m_fragment_shader_cache.clear();
    m_fragment_shader = nullptr;
    m_fsr_easu_shader = nullptr;
    m_fsr_rcas_shader = nullptr;
    m_fsr_pass_shader = nullptr;

    if (m_uam_initialized)
    {
        uam_deinit();
        m_uam_initialized = false;
    }
}

bool Deko3dRenderer::initialize(int frame_width, int frame_height)
{
    if (m_initialized)
        return true;

    m_frame_width = frame_width;
    m_frame_height = frame_height;
    m_display_width = brls::Application::windowWidth;
    m_display_height = brls::Application::windowHeight;

    brls::Logger::info("Deko3dRenderer::initialize: frame={}x{}", frame_width, frame_height);

    m_vctx = (brls::SwitchVideoContext*)brls::Application::getPlatform()->getVideoContext();
    if (!m_vctx)
    {
        brls::Logger::error("Failed to get SwitchVideoContext");
        return false;
    }

    m_device = m_vctx->getDeko3dDevice();
    m_queue = m_vctx->getQueue();

    m_pool_code.emplace(m_device, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code, 128 * 1024);
    m_pool_data.emplace(m_device, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, 1 * 1024 * 1024);

    m_cmdbuf = dk::CmdBufMaker{m_device}.create();
    CMemPool::Handle cmdmem = m_pool_data->allocate(StaticCmdSize);
    m_cmdbuf.addMemory(cmdmem.getMemBlock(), cmdmem.getOffset(), cmdmem.getSize());

    m_update_cmdbuf = dk::CmdBufMaker{m_device}.create();
    m_update_cmdmem = m_pool_data->allocate(UpdateCmdSliceSize * brls::FRAMEBUFFERS_COUNT, DK_CMDMEM_ALIGNMENT);

    m_overlay_cmdbuf = dk::CmdBufMaker{m_device}.create();
    m_overlay_cmdmem.emplace();
    m_overlay_cmdmem->allocate(*m_pool_data, OverlayCmdSliceSize);

    float saved_x = SettingsManager::getInstance()->getStatsOverlayX();
    float saved_y = SettingsManager::getInstance()->getStatsOverlayY();
    if (saved_x >= 0.0f && saved_y >= 0.0f)
    {
        m_overlay_norm_x.store(saved_x, std::memory_order_relaxed);
        m_overlay_norm_y.store(saved_y, std::memory_order_relaxed);
    }

    bool dithering = SettingsManager::getInstance()->getEnableDithering();
    if (!compileVideoShaders(dithering))
    {
        brls::Logger::error("Failed to compile video shaders");
        return false;
    }

    m_vertex_buffer = m_pool_data->allocate(sizeof(QuadVertexData), alignof(Vertex));
    memcpy(m_vertex_buffer.getCpuAddr(), QuadVertexData.data(), m_vertex_buffer.getSize());

    if (!ensureOverlayShaders())
        brls::Logger::warning("Deko3dRenderer: overlay blit shaders unavailable");

    brls::Logger::info("Deko3dRenderer: shaders and vertex buffer initialized");


    m_initialized = true;
    return true;
}

bool Deko3dRenderer::setupTextures(AVFrame* frame)
{
    if (m_textures_initialized)
        return true;

    brls::Logger::info("Deko3dRenderer::setupTextures: frame={}x{}, format={}",
        frame->width, frame->height, static_cast<int>(frame->format));

    if (frame->format != AV_PIX_FMT_NVTEGRA)
    {
        brls::Logger::error("Frame is not NVTEGRA (format={})", static_cast<int>(frame->format));
        return false;
    }

    m_frame_width = frame->width;
    m_frame_height = frame->height;

    m_luma_texture_id = m_vctx->allocateImageIndex();
    m_chroma_texture_id = m_vctx->allocateImageIndex();

    dk::ImageLayoutMaker{m_device}
        .setType(DkImageType_2D)
        .setFormat(DkImageFormat_R8_Unorm)
        .setDimensions(m_frame_width, m_frame_height, 1)
        .setFlags(DkImageFlags_UsageLoadStore | DkImageFlags_Usage2DEngine | DkImageFlags_UsageVideo)
        .initialize(m_luma_layout);

    dk::ImageLayoutMaker{m_device}
        .setType(DkImageType_2D)
        .setFormat(DkImageFormat_RG8_Unorm)
        .setDimensions(m_frame_width / 2, m_frame_height / 2, 1)
        .setFlags(DkImageFlags_UsageLoadStore | DkImageFlags_Usage2DEngine | DkImageFlags_UsageVideo)
        .initialize(m_chroma_layout);

    bool easuSetting = SettingsManager::getInstance()->getEasuEnabled();
    bool rcasSetting = SettingsManager::getInstance()->getRcasEnabled();
    int targetH = SettingsManager::getInstance()->getEasuTargetHeight();
    int targetW = (targetH * 16) / 9;
    bool easuNeeded = easuSetting && targetH > 0 && (m_frame_width < targetW || m_frame_height < targetH);
    m_fsr_pending = easuNeeded || rcasSetting;

    recordStaticVideoCommands();

    m_textures_initialized = true;
    brls::Logger::info("Deko3dRenderer::setupTextures: luma_id={}, chroma_id={}, fsr={}",
        m_luma_texture_id, m_chroma_texture_id, m_fsr_enabled);
    return true;
}

void Deko3dRenderer::releaseTextures()
{
    m_queue.waitIdle();

    if (m_current_frame)
        av_frame_free(&m_current_frame);
    m_frame_bound = false;

    cleanupFsr();
    m_fsr_pending = false;

    if (m_luma_texture_id) {
        m_vctx->freeImageIndex(m_luma_texture_id);
        m_luma_texture_id = 0;
    }
    if (m_chroma_texture_id) {
        m_vctx->freeImageIndex(m_chroma_texture_id);
        m_chroma_texture_id = 0;
    }

    m_frame_mappings.clear();
    m_current_mapping_index = -1;
    m_video_cmdlist = 0;

    m_luma_layout = dk::ImageLayout{};
    m_chroma_layout = dk::ImageLayout{};
    m_textures_initialized = false;
}

void Deko3dRenderer::recordStaticVideoCommands()
{
    m_cmdbuf.clear();

    m_cmdbuf.bindRasterizerState(dk::RasterizerState{}.setCullMode(DkFace_None));
    m_cmdbuf.bindDepthStencilState(dk::DepthStencilState{}
        .setDepthTestEnable(false)
        .setDepthWriteEnable(false)
        .setStencilTestEnable(false));
    m_cmdbuf.bindColorState(dk::ColorState{});
    m_cmdbuf.bindColorWriteState(dk::ColorWriteState{});

    m_cmdbuf.bindShaders(DkStageFlag_GraphicsMask, { m_vertex_shader, *m_fragment_shader });
    m_cmdbuf.bindTextures(DkStage_Fragment, 0, dkMakeTextureHandle(m_luma_texture_id, 0));
    m_cmdbuf.bindTextures(DkStage_Fragment, 1, dkMakeTextureHandle(m_chroma_texture_id, 0));

    m_cmdbuf.bindVtxBuffer(0, m_vertex_buffer.getGpuAddr(), m_vertex_buffer.getSize());
    m_cmdbuf.bindVtxAttribState(VertexAttribState);
    m_cmdbuf.bindVtxBufferState(VertexBufferState);

    m_cmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);

    m_video_cmdlist = m_cmdbuf.finishList();
    brls::Logger::info("Deko3dRenderer: static video command list recorded");
}

bool Deko3dRenderer::compileShaderFromSource(CShader& shader, const std::string& source, bool isVertex)
{
    uint8_t* dksh_out = nullptr;
    uint32_t dksh_size = 0;

    uam_pipeline_stage stage = isVertex ? uam_pipeline_stage_vertex : uam_pipeline_stage_fragment;

    brls::Logger::info("Deko3dRenderer: compiling {} shader ({} bytes)",
        isVertex ? "vertex" : "fragment", source.size());

    if (!uam_compileDksh(stage, source.c_str(), 3, &dksh_out, &dksh_size))
    {
        brls::Logger::error("Deko3dRenderer: failed to compile {} shader",
            isVertex ? "vertex" : "fragment");
        return false;
    }

    brls::Logger::info("Deko3dRenderer: {} shader compiled ({} bytes dksh)",
        isVertex ? "vertex" : "fragment", dksh_size);

    shader.destroy();
    bool result = shader.loadFromMemory(*m_pool_code, dksh_out, dksh_size);
    std::free(dksh_out);

    if (!result)
        brls::Logger::error("Deko3dRenderer: failed to load compiled {} shader into GPU memory",
            isVertex ? "vertex" : "fragment");

    return result;
}

bool Deko3dRenderer::ensureVertexShaderCompiled()
{
    std::string vsh_source = loadShaderSource("romfs:/shaders/video_vsh.glsl");
    if (vsh_source.empty())
        return false;

    return compileShaderFromSource(m_vertex_shader, vsh_source, true);
}

CShader* Deko3dRenderer::getOrCreateFragmentShader(const ShaderVariantKey& key)
{
    for (auto& variant : m_fragment_shader_cache)
    {
        if (variant->key == key)
            return &variant->shader;
    }

    std::string fsh_source = loadShaderSource("romfs:/shaders/video_fsh.glsl");
    if (fsh_source.empty())
        return nullptr;

    auto variant = std::make_unique<CachedFragmentShader>();
    variant->key = key;

    if (!compileShaderFromSource(variant->shader,
        injectShaderDefines(fsh_source,
            buildVideoFragmentDefines(key.dithering, key.fsrEasu, key.fsrRcas, key.fsrPass)), false))
        return nullptr;

    CShader* shader = &variant->shader;
    m_fragment_shader_cache.emplace_back(std::move(variant));
    return shader;
}

bool Deko3dRenderer::compileVideoShaders(bool dithering)
{
    brls::Logger::info("Deko3dRenderer: compiling video shaders (dithering={})", dithering);

    if (!ensureVertexShaderCompiled())
        return false;

    m_fragment_shader = getOrCreateFragmentShader(ShaderVariantKey{
        .dithering = dithering,
    });
    if (!m_fragment_shader)
        return false;

    m_dithering_enabled = dithering;
    brls::Logger::info("Deko3dRenderer: video shaders compiled successfully");
    return true;
}

void Deko3dRenderer::setDithering(bool enabled)
{
    if (enabled == m_dithering_enabled)
        return;

    if (!m_initialized || !m_pool_code)
        return;

    brls::Logger::info("Deko3dRenderer: switching dithering to {}", enabled);

    if (compileVideoShaders(enabled) && m_textures_initialized)
        recordStaticVideoCommands();
}

void Deko3dRenderer::initFsr()
{
    m_queue.waitIdle();

    m_easu_enabled = false;
    m_rcas_enabled = SettingsManager::getInstance()->getRcasEnabled();
    m_fsr_sharpness = SettingsManager::getInstance()->getRcasSharpness();

    int targetH = SettingsManager::getInstance()->getEasuTargetHeight();
    int targetW = (targetH * 16) / 9;
    if (SettingsManager::getInstance()->getEasuEnabled() && targetH > 0 &&
        (m_frame_width < targetW || m_frame_height < targetH))
    {
        m_easu_enabled = true;
        m_fsr_target_height = targetH;
        m_fsr_target_width = targetW;
    }
    else
    {
        m_fsr_target_height = m_display_height;
        m_fsr_target_width = m_display_width;
    }

    if (!m_easu_enabled && !m_rcas_enabled)
        return;

    m_fsr_supersampling = (m_fsr_target_width > m_display_width || m_fsr_target_height > m_display_height);

    brls::Logger::info("Deko3dRenderer::initFsr: input={}x{} target={}x{} display={}x{} ss={} ratio={:.2f}x{:.2f}",
        m_frame_width, m_frame_height, m_fsr_target_width, m_fsr_target_height,
        m_display_width, m_display_height, m_fsr_supersampling,
        (float)m_fsr_target_width / m_frame_width, (float)m_fsr_target_height / m_frame_height);

    if (!ensureVertexShaderCompiled())
        return;

    bool dithering = SettingsManager::getInstance()->getEnableDithering();

    if (m_easu_enabled)
    {
        m_fsr_easu_shader = getOrCreateFragmentShader(ShaderVariantKey{
            .dithering = dithering,
            .fsrEasu = true,
        });
        if (!m_fsr_easu_shader)
            return;
    }
    if (m_rcas_enabled)
    {
        m_fsr_rcas_shader = getOrCreateFragmentShader(ShaderVariantKey{
            .fsrRcas = true,
        });
        if (!m_fsr_rcas_shader)
            return;
    }
    m_fsr_pass_shader = getOrCreateFragmentShader(ShaderVariantKey{
        .fsrPass = true,
    });
    if (!m_fsr_pass_shader)
        return;

    CMemPool* imagesPool = m_vctx->getImagesPool();
    if (!imagesPool)
    {
        brls::Logger::error("initFsr: no images pool available");
        return;
    }

    dk::ImageLayoutMaker{m_device}
        .setType(DkImageType_2D)
        .setFormat(DkImageFormat_RGBA8_Unorm)
        .setDimensions(m_fsr_target_width, m_fsr_target_height, 1)
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_UsageLoadStore | DkImageFlags_Usage2DEngine)
        .initialize(m_rt_easu_layout);

    m_rt_easu_handle = imagesPool->allocate(m_rt_easu_layout.getSize(), m_rt_easu_layout.getAlignment());
    m_rt_easu_image.initialize(m_rt_easu_layout, m_rt_easu_handle.getMemBlock(), m_rt_easu_handle.getOffset());
    m_rt_easu_desc.initialize(m_rt_easu_image, true);
    m_rt_easu_texture_id = m_vctx->allocateImageIndex();

    if (m_fsr_supersampling && m_rcas_enabled)
    {
        m_rt_rcas_handle = imagesPool->allocate(m_rt_easu_layout.getSize(), m_rt_easu_layout.getAlignment());
        m_rt_rcas_image.initialize(m_rt_easu_layout, m_rt_rcas_handle.getMemBlock(), m_rt_rcas_handle.getOffset());
        m_rt_rcas_desc.initialize(m_rt_rcas_image, true);
        m_rt_rcas_texture_id = m_vctx->allocateImageIndex();
    }

    m_fsr_uniform_buffer = m_pool_data->allocate(512, DK_UNIFORM_BUF_ALIGNMENT);
    computeFsrConstants();

    m_fsr_static_cmdbuf = dk::CmdBufMaker{m_device}.create();
    m_fsr_static_cmdmem = m_pool_data->allocate(FsrStaticCmdSize);
    m_fsr_static_cmdbuf.addMemory(m_fsr_static_cmdmem.getMemBlock(), m_fsr_static_cmdmem.getOffset(), m_fsr_static_cmdmem.getSize());

    m_fsr_rcas_cmdbuf = dk::CmdBufMaker{m_device}.create();
    m_fsr_rcas_cmdmem = m_pool_data->allocate(FsrRcasCmdSize);

    m_update_cmdbuf.clear();
    m_update_cmdbuf.addMemory(
        m_update_cmdmem.getMemBlock(),
        m_update_cmdmem.getOffset(),
        UpdateCmdSliceSize);
    if (!m_vctx->updateImageDescriptor(m_update_cmdbuf, m_rt_easu_texture_id, m_rt_easu_desc))
        brls::Logger::error("Deko3dRenderer::initFsr: failed to bind EASU descriptor (id={})", m_rt_easu_texture_id);
    if (m_rt_rcas_texture_id)
    {
        if (!m_vctx->updateImageDescriptor(m_update_cmdbuf, m_rt_rcas_texture_id, m_rt_rcas_desc))
            brls::Logger::error("Deko3dRenderer::initFsr: failed to bind RCAS descriptor (id={})", m_rt_rcas_texture_id);
    }
    m_vctx->invalidateImageDescriptors(m_update_cmdbuf);
    m_queue.submitCommands(m_update_cmdbuf.finishList());
    m_queue.waitIdle();

    recordFsrCommands();

    m_fsr_enabled = true;
    brls::Logger::info("Deko3dRenderer::initFsr: initialized, sharpness={}", m_fsr_sharpness);
}

void Deko3dRenderer::computeFsrConstants()
{
    struct {
        uint32_t con0[4];
        uint32_t con1[4];
        uint32_t con2[4];
        uint32_t con3[4];
        uint32_t rcas_con0[4];
    } constants;

    float inputW = static_cast<float>(m_frame_width);
    float inputH = static_cast<float>(m_frame_height);
    float outputW = static_cast<float>(m_fsr_target_width);
    float outputH = static_cast<float>(m_fsr_target_height);

    auto f2u = [](float f) -> uint32_t {
        uint32_t u;
        memcpy(&u, &f, sizeof(u));
        return u;
    };

    constants.con0[0] = f2u(inputW / outputW);
    constants.con0[1] = f2u(inputH / outputH);
    constants.con0[2] = f2u(0.5f * inputW / outputW - 0.5f);
    constants.con0[3] = f2u(0.5f * inputH / outputH - 0.5f);

    constants.con1[0] = f2u(1.0f / inputW);
    constants.con1[1] = f2u(1.0f / inputH);
    constants.con1[2] = f2u(1.0f / inputW);
    constants.con1[3] = f2u(-1.0f / inputH);

    constants.con2[0] = f2u(-1.0f / inputW);
    constants.con2[1] = f2u(2.0f / inputH);
    constants.con2[2] = f2u(1.0f / inputW);
    constants.con2[3] = f2u(2.0f / inputH);

    constants.con3[0] = f2u(0.0f / inputW);
    constants.con3[1] = f2u(4.0f / inputH);
    constants.con3[2] = 0;
    constants.con3[3] = 0;

    memcpy(m_fsr_uniform_buffer.getCpuAddr(), &constants, sizeof(constants));

    struct { uint32_t con0[4]; } rcas_constants;
    float sharpnessLinear = exp2f(-m_fsr_sharpness);
    rcas_constants.con0[0] = f2u(sharpnessLinear);
    rcas_constants.con0[1] = 0;
    rcas_constants.con0[2] = 0;
    rcas_constants.con0[3] = 0;

    memcpy((uint8_t*)m_fsr_uniform_buffer.getCpuAddr() + 256, &rcas_constants, sizeof(rcas_constants));
}

void Deko3dRenderer::recordFsrCommands()
{
    m_fsr_static_cmdbuf.clear();
    m_fsr_static_cmdbuf.addMemory(m_fsr_static_cmdmem.getMemBlock(), m_fsr_static_cmdmem.getOffset(), m_fsr_static_cmdmem.getSize());

    dk::ImageView easuTarget{m_rt_easu_image};
    m_fsr_static_cmdbuf.bindRenderTargets(&easuTarget);
    m_fsr_static_cmdbuf.setViewports(0, {{ 0.0f, 0.0f, (float)m_fsr_target_width, (float)m_fsr_target_height, 0.0f, 1.0f }});
    m_fsr_static_cmdbuf.setScissors(0, {{ 0, 0, (uint32_t)m_fsr_target_width, (uint32_t)m_fsr_target_height }});

    m_fsr_static_cmdbuf.bindRasterizerState(dk::RasterizerState{}.setCullMode(DkFace_None));
    m_fsr_static_cmdbuf.bindDepthStencilState(dk::DepthStencilState{}
        .setDepthTestEnable(false)
        .setDepthWriteEnable(false)
        .setStencilTestEnable(false));
    m_fsr_static_cmdbuf.bindColorState(dk::ColorState{});
    m_fsr_static_cmdbuf.bindColorWriteState(dk::ColorWriteState{});

    if (m_easu_enabled)
    {
        m_fsr_static_cmdbuf.bindShaders(DkStageFlag_GraphicsMask, { m_vertex_shader, *m_fsr_easu_shader });
        m_fsr_static_cmdbuf.bindTextures(DkStage_Fragment, 0, dkMakeTextureHandle(m_luma_texture_id, 0));
        m_fsr_static_cmdbuf.bindTextures(DkStage_Fragment, 1, dkMakeTextureHandle(m_chroma_texture_id, 0));
        m_fsr_static_cmdbuf.bindUniformBuffer(DkStage_Fragment, 0,
            m_fsr_uniform_buffer.getGpuAddr(), 64);
    }
    else
    {
        m_fsr_static_cmdbuf.bindShaders(DkStageFlag_GraphicsMask, { m_vertex_shader, *m_fragment_shader });
        m_fsr_static_cmdbuf.bindTextures(DkStage_Fragment, 0, dkMakeTextureHandle(m_luma_texture_id, 0));
        m_fsr_static_cmdbuf.bindTextures(DkStage_Fragment, 1, dkMakeTextureHandle(m_chroma_texture_id, 0));
    }
    m_fsr_static_cmdbuf.bindVtxBuffer(0, m_vertex_buffer.getGpuAddr(), m_vertex_buffer.getSize());
    m_fsr_static_cmdbuf.bindVtxAttribState(VertexAttribState);
    m_fsr_static_cmdbuf.bindVtxBufferState(VertexBufferState);
    m_fsr_static_cmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);

    m_fsr_easu_cmdlist = m_fsr_static_cmdbuf.finishList();

    brls::Logger::info("Deko3dRenderer: FSR EASU command list recorded");
}

void Deko3dRenderer::cleanupFsr()
{
    if (!m_fsr_enabled)
        return;

    brls::Logger::info("Deko3dRenderer::cleanupFsr");

    m_fsr_easu_cmdlist = 0;

    m_fsr_static_cmdmem.destroy();
    m_fsr_rcas_cmdmem.destroy();
    m_fsr_uniform_buffer.destroy();

    if (m_rt_easu_texture_id) {
        m_vctx->freeImageIndex(m_rt_easu_texture_id);
        m_rt_easu_texture_id = 0;
    }
    if (m_rt_rcas_texture_id) {
        m_vctx->freeImageIndex(m_rt_rcas_texture_id);
        m_rt_rcas_texture_id = 0;
    }

    m_rt_easu_handle.destroy();
    m_rt_rcas_handle.destroy();
    m_rt_easu_image = dk::Image{};

    m_fsr_easu_shader = nullptr;
    m_fsr_rcas_shader = nullptr;
    m_fsr_pass_shader = nullptr;
    m_easu_enabled = false;
    m_rcas_enabled = false;

    m_fsr_enabled = false;
}

void Deko3dRenderer::draw(AVFrame* frame)
{
    if (!m_initialized)
        return;

    if (!frame || frame->format != AV_PIX_FMT_NVTEGRA)
        return;

    if (m_textures_initialized && (frame->width != m_frame_width || frame->height != m_frame_height))
    {
        brls::Logger::info("Deko3dRenderer: frame size changed from {}x{} to {}x{}, rebuilding textures",
            m_frame_width, m_frame_height, frame->width, frame->height);
        releaseTextures();
    }

    if (!m_textures_initialized)
    {
        if (!setupTextures(frame))
            return;
    }

    if (m_paused.load(std::memory_order_relaxed))
        return;

    AVFrame* new_ref = av_frame_alloc();
    if (!new_ref || av_frame_ref(new_ref, frame) < 0)
    {
        if (new_ref) av_frame_free(&new_ref);
        return;
    }

    if (m_current_frame)
        av_frame_free(&m_current_frame);

    m_current_frame = new_ref;
    m_frame_bound = true;
}

void Deko3dRenderer::presentFrame(AVFrame* frame)
{
    draw(frame);

    if (!m_frame_bound || m_paused.load(std::memory_order_relaxed))
        return;

    VideoContext* videoContext = brls::Application::getPlatform()->getVideoContext();
    videoContext->beginFrame();
    renderVideo();
    videoContext->endFrame();

    recordPresentedFrame();
}

void Deko3dRenderer::recordPresentedFrame()
{
    auto now = std::chrono::steady_clock::now();
    if (!m_render_fps_init)
    {
        m_render_fps_start = now;
        m_render_frame_count = 0;
        m_render_fps_init = true;
    }

    m_render_frame_count++;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_render_fps_start).count();
    if (elapsed >= 1000)
    {
        m_render_fps = (m_render_frame_count * 1000.0f) / elapsed;
        m_render_frame_count = 0;
        m_render_fps_start = now;
    }
}

void Deko3dRenderer::renderVideo()
{
    if (!m_initialized || !m_textures_initialized || !m_frame_bound || !m_current_frame)
        return;

    if (m_fsr_pending)
    {
        m_queue.waitIdle();
        initFsr();
        m_fsr_pending = false;
    }

    if (!m_fsr_enabled && !m_video_cmdlist)
        return;

    AVFrame* bound = m_current_frame;
    {
        std::lock_guard lock(m_frame_ring_mutex);
        const int oldest = m_frame_ring_index;
        if (m_frame_ring[oldest])
            av_frame_free(&m_frame_ring[oldest]);

        m_frame_ring[oldest] = m_current_frame;
        m_current_frame = nullptr;
        m_frame_ring_index = (m_frame_ring_index + 1) % FRAME_RING_SIZE;
    }

    updateFrameBindings(bound);

    if (m_fsr_enabled)
    {
        m_queue.submitCommands(m_fsr_easu_cmdlist);
        m_queue.waitIdle();

        dk::Image* framebuffer = m_vctx->getFramebuffer();
        if (framebuffer)
        {
            m_fsr_rcas_cmdbuf.clear();
            m_fsr_rcas_cmdbuf.addMemory(m_fsr_rcas_cmdmem.getMemBlock(), m_fsr_rcas_cmdmem.getOffset(), m_fsr_rcas_cmdmem.getSize());

            dk::ImageView colorTarget{*framebuffer};
            m_fsr_rcas_cmdbuf.bindRenderTargets(&colorTarget);
            m_fsr_rcas_cmdbuf.setViewports(0, {{ 0.0f, 0.0f, (float)m_display_width, (float)m_display_height, 0.0f, 1.0f }});
            m_fsr_rcas_cmdbuf.setScissors(0, {{ 0, 0, (uint32_t)m_display_width, (uint32_t)m_display_height }});

            m_fsr_rcas_cmdbuf.bindRasterizerState(dk::RasterizerState{}.setCullMode(DkFace_None));
            m_fsr_rcas_cmdbuf.bindDepthStencilState(dk::DepthStencilState{}
                .setDepthTestEnable(false)
                .setDepthWriteEnable(false)
                .setStencilTestEnable(false));
            m_fsr_rcas_cmdbuf.bindColorState(dk::ColorState{});
            m_fsr_rcas_cmdbuf.bindColorWriteState(dk::ColorWriteState{});

            if (m_rcas_enabled)
            {
                m_fsr_rcas_cmdbuf.bindShaders(DkStageFlag_GraphicsMask, { m_vertex_shader, *m_fsr_rcas_shader });
                m_fsr_rcas_cmdbuf.bindTextures(DkStage_Fragment, 0, dkMakeTextureHandle(m_rt_easu_texture_id, 0));
                m_fsr_rcas_cmdbuf.bindUniformBuffer(DkStage_Fragment, 0,
                    m_fsr_uniform_buffer.getGpuAddr() + 256, 16);
            }
            else
            {
                m_fsr_rcas_cmdbuf.bindShaders(DkStageFlag_GraphicsMask, { m_vertex_shader, *m_fsr_pass_shader });
                m_fsr_rcas_cmdbuf.bindTextures(DkStage_Fragment, 0, dkMakeTextureHandle(m_rt_easu_texture_id, 0));
            }
            m_fsr_rcas_cmdbuf.bindVtxBuffer(0, m_vertex_buffer.getGpuAddr(), m_vertex_buffer.getSize());
            m_fsr_rcas_cmdbuf.bindVtxAttribState(VertexAttribState);
            m_fsr_rcas_cmdbuf.bindVtxBufferState(VertexBufferState);

            if (m_fsr_supersampling && m_rcas_enabled)
            {
                dk::ImageView rcasTarget{m_rt_rcas_image};
                m_fsr_rcas_cmdbuf.bindRenderTargets(&rcasTarget);
                m_fsr_rcas_cmdbuf.setViewports(0, {{ 0.0f, 0.0f, (float)m_fsr_target_width, (float)m_fsr_target_height, 0.0f, 1.0f }});
                m_fsr_rcas_cmdbuf.setScissors(0, {{ 0, 0, (uint32_t)m_fsr_target_width, (uint32_t)m_fsr_target_height }});
                m_fsr_rcas_cmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);

                dk::ImageView finalTarget{*framebuffer};
                m_fsr_rcas_cmdbuf.bindRenderTargets(&finalTarget);
                m_fsr_rcas_cmdbuf.setViewports(0, {{ 0.0f, 0.0f, (float)m_display_width, (float)m_display_height, 0.0f, 1.0f }});
                m_fsr_rcas_cmdbuf.setScissors(0, {{ 0, 0, (uint32_t)m_display_width, (uint32_t)m_display_height }});
                m_fsr_rcas_cmdbuf.bindShaders(DkStageFlag_GraphicsMask, { m_vertex_shader, *m_fsr_pass_shader });
                m_fsr_rcas_cmdbuf.bindTextures(DkStage_Fragment, 0, dkMakeTextureHandle(m_rt_rcas_texture_id, 0));
                m_fsr_rcas_cmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);
            }
            else
            {
                m_fsr_rcas_cmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);
            }

            m_queue.submitCommands(m_fsr_rcas_cmdbuf.finishList());
        }
    }
    else
    {
        m_queue.submitCommands(m_video_cmdlist);
    }
    m_queue.flush();

    compositeOverlay();

    if (m_border_flash_frames.load(std::memory_order_acquire) > 0)
        renderBorderFlash();
}

void Deko3dRenderer::registerCallback()
{
    if (m_callback_registered)
        return;

    brls::Application::setPostRenderCallback([this]() {
        if (m_tick_callback)
            m_tick_callback();

        if (m_frame_bound && !m_paused.load(std::memory_order_relaxed))
        {
            renderVideo();
        }
    });

    m_callback_registered = true;
    brls::Logger::info("Deko3dRenderer: post-render callback registered");
}

void Deko3dRenderer::unregisterCallback()
{
    if (!m_callback_registered)
        return;

    brls::Application::setExclusiveRender(false);
    brls::Application::setPostRenderCallback(nullptr);
    m_callback_registered = false;
    brls::Logger::info("Deko3dRenderer: post-render callback unregistered");
}

void Deko3dRenderer::updateFrameBindings(AVFrame* frame)
{
    AVNVTegraMap* map = av_nvtegra_frame_get_fbuf_map(frame);
    if (!map)
        return;

    uint32_t handle = av_nvtegra_map_get_handle(map);
    void* cpuAddr = av_nvtegra_map_get_addr(map);
    uint32_t size = av_nvtegra_map_get_size(map);
    uint32_t chromaOffset = static_cast<uint32_t>(frame->data[1] - frame->data[0]);

    int mappingIndex = -1;
    for (size_t i = 0; i < m_frame_mappings.size(); ++i)
    {
        const auto& m = m_frame_mappings[i];
        if (m.handle == handle && m.cpuAddr == cpuAddr && m.size == size && m.chromaOffset == chromaOffset)
        {
            mappingIndex = static_cast<int>(i);
            break;
        }
    }

    if (mappingIndex < 0)
    {
        FrameMapping mapping;
        mapping.handle = handle;
        mapping.cpuAddr = cpuAddr;
        mapping.size = size;
        mapping.chromaOffset = chromaOffset;

        mapping.memblock = dk::MemBlockMaker{m_device, size}
            .setFlags(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)
            .setStorage(cpuAddr)
            .create();

        mapping.luma.initialize(m_luma_layout, mapping.memblock, 0);
        mapping.chroma.initialize(m_chroma_layout, mapping.memblock, chromaOffset);

        mapping.lumaDesc.initialize(mapping.luma);
        mapping.chromaDesc.initialize(mapping.chroma);

        m_frame_mappings.emplace_back(std::move(mapping));
        mappingIndex = static_cast<int>(m_frame_mappings.size()) - 1;
    }

    if (mappingIndex == m_current_mapping_index)
        return;

    m_update_cmdbuf.clear();
    m_update_cmdbuf.addMemory(
        m_update_cmdmem.getMemBlock(),
        m_update_cmdmem.getOffset() + m_update_cmdmem_slice * UpdateCmdSliceSize,
        UpdateCmdSliceSize);
    m_update_cmdmem_slice = (m_update_cmdmem_slice + 1) % brls::FRAMEBUFFERS_COUNT;

    auto& active = m_frame_mappings[mappingIndex];
    const bool updatedLuma = m_vctx->updateImageDescriptor(m_update_cmdbuf, m_luma_texture_id, active.lumaDesc);
    const bool updatedChroma = m_vctx->updateImageDescriptor(m_update_cmdbuf, m_chroma_texture_id, active.chromaDesc);

    if (!updatedLuma || !updatedChroma)
    {
        brls::Logger::error("Deko3dRenderer::updateFrameMapping: failed to bind luma/chroma descriptors (luma={}, chroma={})",
                            updatedLuma, updatedChroma);
    }
    else
    {
        m_vctx->invalidateImageDescriptors(m_update_cmdbuf);
        m_current_mapping_index = mappingIndex;
    }

    m_queue.submitCommands(m_update_cmdbuf.finishList());
}

void Deko3dRenderer::cleanup()
{
    destroyOverlayTarget();
    destroyOverlayContext();

    if (!m_initialized)
        return;

    brls::Logger::info("Deko3dRenderer::cleanup");

    unregisterCallback();

    m_queue.waitIdle();

    if (m_current_frame)
        av_frame_free(&m_current_frame);

    {
        std::lock_guard lock(m_frame_ring_mutex);
        for (int i = 0; i < FRAME_RING_SIZE; ++i)
        {
            if (m_frame_ring[i])
                av_frame_free(&m_frame_ring[i]);
        }
        m_frame_ring_index = 0;
    }

    cleanupFsr();

    m_vertex_buffer.destroy();

    if (m_luma_texture_id) {
        m_vctx->freeImageIndex(m_luma_texture_id);
        m_luma_texture_id = 0;
    }
    if (m_chroma_texture_id) {
        m_vctx->freeImageIndex(m_chroma_texture_id);
        m_chroma_texture_id = 0;
    }

    m_frame_mappings.clear();
    m_current_mapping_index = -1;

    m_video_cmdlist = 0;
    m_update_cmdmem.destroy();
    m_overlay_cmdmem.reset();

    m_luma_layout = dk::ImageLayout{};
    m_chroma_layout = dk::ImageLayout{};

    m_frame_bound = false;

    m_initialized = false;
    m_textures_initialized = false;
}

void Deko3dRenderer::waitIdle()
{
    if (m_initialized)
    {
        m_queue.waitIdle();
    }
}



void Deko3dRenderer::warmFontAtlas(NVGcontext* vg)
{
    if (m_font_atlas_warm)
        return;

    static const char* charset =
        "0123456789.,:/%+-x ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz·×›";

    nvgFontFaceId(vg, m_overlay_font);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 0));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

    for (float size : { 9.5f, 10.0f, 10.5f, 11.5f, 12.0f, 12.5f, 13.0f, 14.0f })
    {
        nvgFontSize(vg, ovlSize(size * m_overlay_scale));
        nvgText(vg, -4096.0f, -4096.0f, charset, nullptr);
    }

    m_font_atlas_warm = true;
}

void Deko3dRenderer::rebuildOverlayText()
{
    const StreamStats m_stats = readStreamStats();

    uint64_t mins = m_stats.stream_duration_seconds / 60;
    uint64_t secs = m_stats.stream_duration_seconds % 60;

    auto& wg = WireGuardManager::instance();

    m_ov.fps = std::format("{:.1f}", m_render_fps);
    m_ov.rate = std::format("{:.1f}", m_stats.measured_bitrate_mbps);
    m_ov.loss = std::format("{:.1f}", m_stats.packet_loss_percent);
    m_ov.rtt = m_stats.rtt_valid ? std::format("{:.0f}", m_stats.rtt_ms) : std::string("--");
    m_ov.uptime = std::format("{}m{:02}s", mins, secs);

    m_ov.req_res = std::format("{}x{}", m_stats.requested_width, m_stats.requested_height);
    m_ov.req_fps = std::format("{}", m_stats.requested_fps);
    m_ov.req_rate = std::format("{:.1f}", m_stats.requested_bitrate / 1000.0f);
    m_ov.req_codec = m_stats.requested_hevc ? "HEVC" : "H.264";

    m_ov.out_res = std::format("{}x{}", m_stats.video_width, m_stats.video_height);
    m_ov.out_codec = m_stats.is_hevc ? "HEVC" : "H.264";
    m_ov.out_path = m_stats.is_hardware_decoder ? "NVTEGRA" : "SW";

    m_ov.fsr_active = m_fsr_enabled;
    m_ov.fsr_path.clear();
    m_ov.fsr_detail.clear();
    m_ov.fsr_compact.clear();
    if (m_ov.fsr_active)
    {
        if (m_easu_enabled)
        {
            m_ov.fsr_path = std::format("{}›{}›{}",
                m_frame_height, m_fsr_target_height, m_display_height);
            const float scale = m_frame_height > 0
                ? static_cast<float>(m_fsr_target_height) / static_cast<float>(m_frame_height)
                : 1.0f;
            m_ov.fsr_detail = "EASU";
            m_ov.fsr_compact = std::format("{:.2f}× {}", scale,
                m_fsr_supersampling ? "SS" : "EASU");
        }
        else
        {
            m_ov.fsr_path = std::format("{}›{}", m_frame_height, m_display_height);
            m_ov.fsr_compact = "RCAS";
        }

        if (m_rcas_enabled)
        {
            const int strength = std::clamp(static_cast<int>(std::lround(
                (1.0f - m_fsr_sharpness / 2.0f) * 100.0f)), 0, 100);
            const std::string rcas = std::format("RCAS {}%", strength);
            if (!m_ov.fsr_detail.empty())
                m_ov.fsr_detail += " · ";
            m_ov.fsr_detail += rcas;
            if (m_easu_enabled)
                m_ov.fsr_compact += std::format(" · {}", rcas);
            else
                m_ov.fsr_compact = rcas;
        }
    }

    m_ov.lost = std::format("{}", m_stats.network_frames_lost);
    m_ov.recovered = std::format("{}", m_stats.frames_recovered);

    m_ov.ghash = "PMULL";
    m_ov.vpn = wg.isConnected() ? wg.getTunnelIP() : "Off";

    m_ov.context = m_connection_context.empty()
        ? std::format("{} \xc2\xb7 {}",
            m_stats.is_hevc ? "PS5" : "PS4",
            wg.isConnected() ? "VPN" : "Direct")
        : m_connection_context;

    m_ov.lat_valid = m_stats.latency_valid;
    m_ov.lat_net_ms = m_stats.net_ms;
    m_ov.lat_visual_ms = m_stats.visual_ms;
    m_ov.lat_total_ms = m_stats.total_ms;

    m_ov.lat_net = m_stats.latency_valid ? std::format("{:.0f}", m_stats.net_ms) : std::string("--");
    m_ov.lat_visual = std::format("{:.1f}", m_stats.visual_ms);
    m_ov.lat_total = m_stats.latency_valid ? std::format("{:.0f}", m_stats.total_ms) : std::string("--");
    m_ov.lat_jitter = std::format("{:.1f}", m_stats.jitter_ms);
    m_ov.lat_decode = std::format("{:.1f}", m_stats.decode_ms);
    m_ov.src_fps = std::format("{:.1f}", m_stats.source_fps);
}

namespace
{
    NVGcolor ovl(NVGcolor c, unsigned char a)
    {
        return akira::ui::withAlpha(c, a);
    }
}

Deko3dRenderer::Tone Deko3dRenderer::toneFps() const
{
    if (m_stats.requested_fps <= 0)
        return Tone::Good;
    float r = m_render_fps / (float)m_stats.requested_fps;
    return r >= 0.92f ? Tone::Good : (r >= 0.75f ? Tone::Warn : Tone::Bad);
}

Deko3dRenderer::Tone Deko3dRenderer::toneLoss() const
{
    float l = m_stats.packet_loss_percent;
    return l < 0.5f ? Tone::Good : (l < 2.0f ? Tone::Warn : Tone::Bad);
}

Deko3dRenderer::Tone Deko3dRenderer::toneRate() const
{
    if (m_stats.requested_bitrate <= 0)
        return Tone::Good;
    float r = m_stats.measured_bitrate_mbps / (m_stats.requested_bitrate / 1000.0f);
    return r >= 0.7f ? Tone::Good : (r >= 0.4f ? Tone::Warn : Tone::Bad);
}

Deko3dRenderer::Tone Deko3dRenderer::toneRtt() const
{
    if (!m_stats.rtt_valid)
        return Tone::Good;
    float v = m_stats.rtt_ms;
    return v < 40.0f ? Tone::Good : (v < 80.0f ? Tone::Warn : Tone::Bad);
}

Deko3dRenderer::Tone Deko3dRenderer::toneLost() const
{
    size_t v = m_stats.network_frames_lost;
    return v == 0 ? Tone::Good : (v <= 20 ? Tone::Warn : Tone::Bad);
}

Deko3dRenderer::Tone Deko3dRenderer::toneLatency() const
{
    if (!m_stats.latency_valid)
        return Tone::Good;
    float v = m_stats.total_ms;
    return v < 50.0f ? Tone::Good : (v < 100.0f ? Tone::Warn : Tone::Bad);
}

NVGcolor Deko3dRenderer::toneColor(Tone tone) const
{
    switch (tone)
    {
        case Tone::Warn: return akira::ui::active().warning;
        case Tone::Bad:  return akira::ui::active().danger;
        default:         return ovl(akira::ui::active().text, 242);
    }
}

void Deko3dRenderer::drawOverlayPanel(NVGcontext* vg, float x, float y, float w, float h)
{
    float s = m_overlay_scale;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, 10.0f * s);
    nvgFillColor(vg, ovl(akira::ui::active().backgroundDeep, 148));
    nvgFill(vg);
    nvgStrokeWidth(vg, 1.0f);
    nvgStrokeColor(vg, ovl(akira::ui::active().text, 38));
    nvgStroke(vg);
}

void Deko3dRenderer::drawLatencyStrip(NVGcontext* vg, float x, float y, float w, float h)
{
    float s = m_overlay_scale;
    const akira::ui::Palette& p = akira::ui::active();
    (void)h;

    nvgFontSize(vg, ovlSize(9.5f * s));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, ovl(p.text, 87));
    nvgText(vg, x, y + 9.0f * s, "LATENCY", nullptr);

    nvgFontSize(vg, ovlSize(10.0f * s));
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, ovl(p.text, 117));
    nvgText(vg, x + w, y + 9.0f * s, "ms", nullptr);
    float tright = x + w - nvgTextBounds(vg, 0, 0, "ms", nullptr, nullptr) - 3.0f * s;

    nvgFontSize(vg, ovlSize(13.0f * s));
    nvgFillColor(vg, toneColor(toneLatency()));
    nvgText(vg, tright, y + 9.0f * s, m_ov.lat_total.c_str(), nullptr);

    float barY = y + 19.0f * s;
    float barH = 7.0f * s;
    float radius = barH * 0.5f;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, barY, w, barH, radius);
    nvgFillColor(vg, ovl(p.text, 20));
    nvgFill(vg);

    const float reference_ms = 120.0f;
    float total = m_ov.lat_valid ? m_ov.lat_total_ms : m_ov.lat_visual_ms;
    float fill = std::clamp(total / reference_ms, 0.0f, 1.0f) * w;

    if (fill > 1.0f)
    {
        float netW = 0.0f;
        if (m_ov.lat_valid && total > 0.0f)
            netW = fill * std::clamp(m_ov.lat_net_ms / total, 0.0f, 1.0f);

        nvgSave(vg);
        nvgScissor(vg, x, barY, fill, barH);

        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, barY, fill, barH, radius);
        nvgFillColor(vg, ovl(p.media, 224));
        nvgFill(vg);

        if (netW > 0.0f)
        {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, x, barY, netW, barH, radius);
            nvgFillColor(vg, ovl(p.accent, 235));
            nvgFill(vg);
        }

        nvgRestore(vg);
    }

    struct Legend { const char* key; const std::string& value; NVGcolor dot; bool dotted; };
    Legend legend[] = {
        { "Net",    m_ov.lat_net,    p.accent, true  },
        { "Visual", m_ov.lat_visual, p.media,  true  },
        { "Decode", m_ov.lat_decode, p.text,   false },
        { "Jitter", m_ov.lat_jitter, p.text,   false },
    };

    float lx = x;
    float ly = y + 36.0f * s;
    for (const Legend& l : legend)
    {
        if (l.dotted)
        {
            nvgBeginPath(vg);
            nvgCircle(vg, lx + 3.0f * s, ly, 3.0f * s);
            nvgFillColor(vg, l.dot);
            nvgFill(vg);
            lx += 10.0f * s;
        }

        nvgFontSize(vg, ovlSize(9.5f * s));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, ovl(p.text, 100));
        nvgText(vg, lx, ly, l.key, nullptr);
        lx += nvgTextBounds(vg, 0, 0, l.key, nullptr, nullptr) + 5.0f * s;

        nvgFontSize(vg, ovlSize(10.5f * s));
        nvgFillColor(vg, ovl(p.text, 200));
        nvgText(vg, lx, ly, l.value.c_str(), nullptr);
        lx += nvgTextBounds(vg, 0, 0, l.value.c_str(), nullptr, nullptr) + 16.0f * s;
    }
}

void Deko3dRenderer::drawStatRow(NVGcontext* vg, float x, float y, float w,
                                 const char* label, const std::string& value,
                                 const char* unit, Tone tone)
{
    float s = m_overlay_scale;

    nvgFontSize(vg, ovlSize(11.5f * s));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, ovl(akira::ui::active().text, 117));
    nvgText(vg, x, y, label, nullptr);

    float right = x + w;
    if (unit && *unit)
    {
        nvgFontSize(vg, ovlSize(10.0f * s));
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, ovl(akira::ui::active().text, 117));
        nvgText(vg, right, y, unit, nullptr);
        right -= nvgTextBounds(vg, 0, 0, unit, nullptr, nullptr) + 3.0f * s;
    }

    nvgFontSize(vg, ovlSize(12.5f * s));
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, toneColor(tone));
    nvgText(vg, right, y, value.c_str(), nullptr);
}

void Deko3dRenderer::drawFsrRow(NVGcontext* vg, float x, float y, float w)
{
    float s = m_overlay_scale;
    const auto& p = akira::ui::active();

    nvgFontSize(vg, ovlSize(11.5f * s));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, ovl(p.text, 117));
    nvgText(vg, x, y - 4.0f * s, "FSR", nullptr);

    const float dotX = x + 30.0f * s;
    nvgBeginPath(vg);
    nvgCircle(vg, dotX, y - 4.0f * s, 2.5f * s);
    nvgFillColor(vg, p.success);
    nvgFill(vg);

    nvgFontSize(vg, ovlSize(10.5f * s));
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, p.accent);
    nvgText(vg, x + w, y - 4.0f * s, m_ov.fsr_path.c_str(), nullptr);

    nvgFontSize(vg, ovlSize(8.0f * s));
    nvgFillColor(vg, ovl(p.text, 100));
    nvgText(vg, x + w, y + 6.0f * s, m_ov.fsr_detail.c_str(), nullptr);
}

void Deko3dRenderer::drawCompactCell(NVGcontext* vg, float& cursorX, float centerY,
                                     const char* label, const std::string& value,
                                     const char* unit, Tone tone, bool first)
{
    float s = m_overlay_scale;
    float pad = 14.0f * s;

    if (!first)
    {
        nvgBeginPath(vg);
        nvgMoveTo(vg, cursorX, centerY - 10.0f * s);
        nvgLineTo(vg, cursorX, centerY + 10.0f * s);
        nvgStrokeWidth(vg, 1.0f);
        nvgStrokeColor(vg, ovl(akira::ui::active().text, 26));
        nvgStroke(vg);
        cursorX += pad;
    }

    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    nvgFontSize(vg, ovlSize(10.0f * s));
    nvgFillColor(vg, ovl(akira::ui::active().text, 117));
    nvgText(vg, cursorX, centerY, label, nullptr);
    cursorX += nvgTextBounds(vg, 0, 0, label, nullptr, nullptr) + 7.0f * s;

    nvgFontSize(vg, ovlSize(14.0f * s));
    nvgFillColor(vg, toneColor(tone));
    nvgText(vg, cursorX, centerY, value.c_str(), nullptr);
    cursorX += nvgTextBounds(vg, 0, 0, value.c_str(), nullptr, nullptr);

    if (unit && *unit)
    {
        cursorX += 3.0f * s;
        nvgFontSize(vg, ovlSize(10.5f * s));
        nvgFillColor(vg, ovl(akira::ui::active().text, 117));
        nvgText(vg, cursorX, centerY, unit, nullptr);
        cursorX += nvgTextBounds(vg, 0, 0, unit, nullptr, nullptr);
    }

    cursorX += pad;
}

void Deko3dRenderer::drawCompactFsrLine(NVGcontext* vg, float x, float centerY)
{
    float s = m_overlay_scale;
    const auto& p = akira::ui::active();

    nvgBeginPath(vg);
    nvgCircle(vg, x + 2.5f * s, centerY, 2.5f * s);
    nvgFillColor(vg, p.success);
    nvgFill(vg);
    x += 9.0f * s;

    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFontSize(vg, ovlSize(10.0f * s));
    nvgFillColor(vg, p.accent);
    nvgText(vg, x, centerY, "FSR", nullptr);
    x += nvgTextBounds(vg, 0, 0, "FSR", nullptr, nullptr) + 7.0f * s;

    nvgFontSize(vg, ovlSize(10.5f * s));
    nvgFillColor(vg, ovl(p.text, 160));
    nvgText(vg, x, centerY, m_ov.fsr_compact.c_str(), nullptr);
}

void Deko3dRenderer::overlayOrigin(float w, float h, float& ox, float& oy)
{
    m_overlay_w.store(w, std::memory_order_relaxed);
    m_overlay_h.store(h, std::memory_order_relaxed);
    updateOverlayPlacement();

    ox = m_overlay_to_texture ? 0.0f : m_overlay_x.load(std::memory_order_relaxed);
    oy = m_overlay_to_texture ? 0.0f : m_overlay_y.load(std::memory_order_relaxed);
}

void Deko3dRenderer::updateOverlayPlacement()
{
    const float w = m_overlay_w.load(std::memory_order_relaxed);
    const float h = m_overlay_h.load(std::memory_order_relaxed);
    if (w <= 0.0f || h <= 0.0f)
        return;

    const float sw = (float)brls::Application::windowWidth;
    const float sh = (float)brls::Application::windowHeight;
    const float margin = 24.0f * m_overlay_scale;

    const float nx = m_overlay_norm_x.load(std::memory_order_relaxed);
    const float ny = m_overlay_norm_y.load(std::memory_order_relaxed);

    float sx = nx < 0.0f ? margin : nx * sw;
    float sy = ny < 0.0f ? margin : ny * sh;

    sx = std::clamp(sx, margin, std::max(margin, sw - w - margin));
    sy = std::clamp(sy, margin, std::max(margin, sh - h - margin));

    m_overlay_x.store(sx, std::memory_order_relaxed);
    m_overlay_y.store(sy, std::memory_order_relaxed);
}

bool Deko3dRenderer::overlayTouchBegin(float x, float y)
{
    std::lock_guard<std::mutex> lock(m_overlay_touch_mutex);
    if (m_stats_mode.load(std::memory_order_relaxed) == StatsOverlayMode::Off)
        return false;

    float ox = m_overlay_x.load(std::memory_order_relaxed);
    float oy = m_overlay_y.load(std::memory_order_relaxed);
    float w = m_overlay_w.load(std::memory_order_relaxed);
    float h = m_overlay_h.load(std::memory_order_relaxed);
    if (w <= 0.0f || h <= 0.0f)
        return false;

    float slop = 12.0f * m_overlay_hit_scale.load(std::memory_order_relaxed);
    if (x < ox - slop || x > ox + w + slop || y < oy - slop || y > oy + h + slop)
    {
        return false;
    }

    m_overlay_grab_dx = x - ox;
    m_overlay_grab_dy = y - oy;
    m_overlay_drag = true;
    return true;
}

void Deko3dRenderer::overlayTouchMove(float x, float y)
{
    std::lock_guard<std::mutex> lock(m_overlay_touch_mutex);
    if (!m_overlay_drag)
        return;

    float sw = (float)brls::Application::windowWidth;
    float sh = (float)brls::Application::windowHeight;
    if (sw <= 0.0f || sh <= 0.0f)
        return;

    m_overlay_norm_x.store((x - m_overlay_grab_dx) / sw, std::memory_order_relaxed);
    m_overlay_norm_y.store((y - m_overlay_grab_dy) / sh, std::memory_order_relaxed);
}

void Deko3dRenderer::overlayTouchEnd()
{
    std::lock_guard<std::mutex> lock(m_overlay_touch_mutex);
    if (!m_overlay_drag)
        return;
    m_overlay_drag = false;

    float sw = (float)brls::Application::windowWidth;
    float sh = (float)brls::Application::windowHeight;
    if (sw <= 0.0f || sh <= 0.0f)
        return;

    SettingsManager::getInstance()->setStatsOverlayPosition(
        m_overlay_x.load(std::memory_order_relaxed) / sw,
        m_overlay_y.load(std::memory_order_relaxed) / sh);
    m_overlay_pos_dirty.store(true, std::memory_order_relaxed);
}

bool Deko3dRenderer::takeOverlayPositionDirty()
{
    return m_overlay_pos_dirty.exchange(false, std::memory_order_relaxed);
}

void Deko3dRenderer::drawCompactOverlay(NVGcontext* vg)
{
    float s = m_overlay_scale;
    float margin = 24.0f * s;
    float height = (m_ov.fsr_active ? 58.0f : 36.0f) * s;

    Tone overall = Tone::Good;
    for (Tone t : { toneFps(), toneRate(), toneLoss(), toneRtt(), toneLost(), toneLatency() })
        if (t > overall) overall = t;

    float probe = margin + 14.0f * s;
    float startX = probe;
    drawCompactCell(vg, probe, -10000.0f, "FPS", m_ov.fps, nullptr, Tone::Good, true);
    drawCompactCell(vg, probe, -10000.0f, "RATE", m_ov.rate, "Mbps", Tone::Good, false);
    drawCompactCell(vg, probe, -10000.0f, "LOSS", m_ov.loss, "%", Tone::Good, false);
    drawCompactCell(vg, probe, -10000.0f, "RTT", m_ov.rtt, "ms", Tone::Good, false);
    drawCompactCell(vg, probe, -10000.0f, "LAT", m_ov.lat_total, "ms", Tone::Good, false);
    drawCompactCell(vg, probe, -10000.0f, "UP", m_ov.uptime, nullptr, Tone::Good, false);
    float width = (probe - startX) + 34.0f * s + 13.0f * s;

    float ox, oy;
    overlayOrigin(width, height, ox, oy);

    drawOverlayPanel(vg, ox, oy, width, height);

    float centerY = oy + 18.0f * s;
    float dotX = ox + 14.0f * s + 3.5f * s;
    nvgBeginPath(vg);
    nvgCircle(vg, dotX, centerY, 3.5f * s);
    nvgFillColor(vg, overall == Tone::Good ? akira::ui::active().success : toneColor(overall));
    nvgFill(vg);

    float cursorX = ox + 14.0f * s + 20.0f * s;
    drawCompactCell(vg, cursorX, centerY, "FPS", m_ov.fps, nullptr, toneFps(), true);
    drawCompactCell(vg, cursorX, centerY, "RATE", m_ov.rate, "Mbps", toneRate(), false);
    drawCompactCell(vg, cursorX, centerY, "LOSS", m_ov.loss, "%", toneLoss(), false);
    drawCompactCell(vg, cursorX, centerY, "RTT", m_ov.rtt, "ms", toneRtt(), false);
    drawCompactCell(vg, cursorX, centerY, "LAT", m_ov.lat_total, "ms", toneLatency(), false);
    drawCompactCell(vg, cursorX, centerY, "UP", m_ov.uptime, nullptr, Tone::Good, false);
    if (m_ov.fsr_active)
    {
        const float dividerY = oy + 36.0f * s;
        nvgBeginPath(vg);
        nvgMoveTo(vg, ox + 14.0f * s, dividerY);
        nvgLineTo(vg, ox + width - 14.0f * s, dividerY);
        nvgStrokeWidth(vg, 1.0f);
        nvgStrokeColor(vg, ovl(akira::ui::active().text, 26));
        nvgStroke(vg);
        drawCompactFsrLine(vg, ox + 14.0f * s, oy + 47.0f * s);
    }
}

void Deko3dRenderer::drawFullOverlay(NVGcontext* vg)
{
    float s = m_overlay_scale;
    float width = 452.0f * s;
    float padX = 16.0f * s;
    float headH = 34.0f * s;
    float rowH = 20.0f * s;
    float footH = 28.0f * s;
    float latH = 48.0f * s;
    const int rowCount = m_ov.fsr_active ? 5 : 4;
    float height = headH + 22.0f * s + rowH * rowCount + 10.0f * s + latH + footH;

    float ox, oy;
    overlayOrigin(width, height, ox, oy);
    float colTop = oy + headH + 22.0f * s;

    drawOverlayPanel(vg, ox, oy, width, height);

    nvgFontSize(vg, ovlSize(12.5f * s));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, akira::ui::active().accent);
    nvgText(vg, ox + padX, oy + headH * 0.5f, "AKIRA", nullptr);
    float bx = ox + padX + nvgTextBounds(vg, 0, 0, "AKIRA", nullptr, nullptr) + 9.0f * s;
    nvgFillColor(vg, ovl(akira::ui::active().text, 56));
    nvgText(vg, bx, oy + headH * 0.5f, "/", nullptr);
    nvgFillColor(vg, ovl(akira::ui::active().text, 184));
    nvgText(vg, bx + 12.0f * s, oy + headH * 0.5f, m_ov.context.c_str(), nullptr);

    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFontSize(vg, ovlSize(12.0f * s));
    nvgFillColor(vg, ovl(akira::ui::active().text, 117));
    nvgText(vg, ox + width - padX, oy + headH * 0.5f, m_ov.uptime.c_str(), nullptr);

    nvgBeginPath(vg);
    nvgMoveTo(vg, ox, oy + headH);
    nvgLineTo(vg, ox + width, oy + headH);
    nvgStrokeWidth(vg, 1.0f);
    nvgStrokeColor(vg, ovl(akira::ui::active().text, 26));
    nvgStroke(vg);

    float colW = (width - padX * 2.0f - 28.0f * s) / 3.0f;
    const char* headers[3] = { "REQUESTED", "OUTPUT", "LINK" };

    for (int c = 0; c < 3; c++)
    {
        float cx = ox + padX + (colW + 14.0f * s) * c;

        if (c > 0)
        {
            nvgBeginPath(vg);
            nvgMoveTo(vg, cx - 7.0f * s, colTop - 14.0f * s);
            nvgLineTo(vg, cx - 7.0f * s, colTop + rowH * rowCount - 4.0f * s);
            nvgStrokeWidth(vg, 1.0f);
            nvgStrokeColor(vg, ovl(akira::ui::active().text, 26));
            nvgStroke(vg);
        }

        nvgFontSize(vg, ovlSize(9.5f * s));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, ovl(akira::ui::active().text, 87));
        nvgText(vg, cx, colTop - 14.0f * s, headers[c], nullptr);

        for (int r = 0; r < rowCount; r++)
        {
            float ry = colTop + rowH * r + rowH * 0.5f;
            if (c == 0)
            {
                if (r == 0) drawStatRow(vg, cx, ry, colW, "Res", m_ov.req_res, nullptr, Tone::Good);
                if (r == 1) drawStatRow(vg, cx, ry, colW, "FPS", m_ov.req_fps, nullptr, Tone::Good);
                if (r == 2) drawStatRow(vg, cx, ry, colW, "Rate", m_ov.req_rate, "Mb", Tone::Good);
                if (r == 3) drawStatRow(vg, cx, ry, colW, "Codec", m_ov.req_codec, nullptr, Tone::Good);
            }
            else if (c == 1)
            {
                if (r == 0) drawStatRow(vg, cx, ry, colW, "Res", m_ov.out_res, nullptr, Tone::Good);
                const int offset = m_ov.fsr_active ? 1 : 0;
                if (m_ov.fsr_active && r == 1) drawFsrRow(vg, cx, ry, colW);
                if (r == 1 + offset) drawStatRow(vg, cx, ry, colW, "FPS", m_ov.fps, nullptr, toneFps());
                if (r == 2 + offset) drawStatRow(vg, cx, ry, colW, "Path", m_ov.out_path, nullptr, Tone::Good);
                if (r == 3 + offset) drawStatRow(vg, cx, ry, colW, "Codec", m_ov.out_codec, nullptr, Tone::Good);
            }
            else
            {
                if (r == 0) drawStatRow(vg, cx, ry, colW, "RTT", m_ov.rtt, "ms", toneRtt());
                if (r == 1) drawStatRow(vg, cx, ry, colW, "Rate", m_ov.rate, "Mb", toneRate());
                if (r == 2) drawStatRow(vg, cx, ry, colW, "Loss", m_ov.loss, "%", toneLoss());
                if (r == 3) drawStatRow(vg, cx, ry, colW, "Lost", m_ov.lost, nullptr, toneLost());
            }
        }
    }

    float latY = colTop + rowH * rowCount + 10.0f * s;
    nvgBeginPath(vg);
    nvgMoveTo(vg, ox, latY);
    nvgLineTo(vg, ox + width, latY);
    nvgStrokeWidth(vg, 1.0f);
    nvgStrokeColor(vg, ovl(akira::ui::active().text, 26));
    nvgStroke(vg);

    drawLatencyStrip(vg, ox + padX, latY + 6.0f * s, width - padX * 2.0f, latH);

    float footY = oy + height - footH;
    nvgBeginPath(vg);
    nvgMoveTo(vg, ox, footY);
    nvgLineTo(vg, ox + width, footY);
    nvgStrokeWidth(vg, 1.0f);
    nvgStrokeColor(vg, ovl(akira::ui::active().text, 26));
    nvgStroke(vg);

    nvgFontSize(vg, ovlSize(10.5f * s));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    float fx = ox + padX;
    float fy = footY + footH * 0.5f;

    struct { const char* k; const std::string& v; } foot[] = {
        { "SRC", m_ov.src_fps },
        { "GHASH", m_ov.ghash },
        { "VPN", m_ov.vpn },
        { "REC", m_ov.recovered },
    };

    for (const auto& f : foot)
    {
        nvgFillColor(vg, ovl(akira::ui::active().text, 117));
        nvgText(vg, fx, fy, f.k, nullptr);
        fx += nvgTextBounds(vg, 0, 0, f.k, nullptr, nullptr) + 6.0f * s;
        nvgFillColor(vg, ovl(akira::ui::active().text, 189));
        nvgText(vg, fx, fy, f.v.c_str(), nullptr);
        fx += nvgTextBounds(vg, 0, 0, f.v.c_str(), nullptr, nullptr) + 18.0f * s;
    }
}

void Deko3dRenderer::updateOverlayTexture()
{
    if (m_stats_mode.load(std::memory_order_relaxed) == StatsOverlayMode::Off)
        return;

    unsigned screenH = brls::Application::windowHeight;
    if (screenH == 0)
        return;

    float scale = (float)screenH / 720.0f;
    if (!(scale > 0.1f && scale < 8.0f))
        scale = 1.0f;

    if (scale != m_overlay_scale)
    {
        m_overlay_scale = scale;
        m_overlay_hit_scale.store(scale, std::memory_order_relaxed);
        m_ovl_dirty = true;
    }

    if (!ensureOverlayTarget())
        return;

    auto now = std::chrono::steady_clock::now();
    if (m_ov_last_rebuild == std::chrono::steady_clock::time_point{} ||
        std::chrono::duration_cast<std::chrono::milliseconds>(now - m_ov_last_rebuild).count() >= 250)
    {
        rebuildOverlayText();
        m_ov_last_rebuild = now;
        m_ovl_dirty = true;
    }

    updateOverlayPlacement();

    if (!m_ovl_dirty)
        return;

    const int front = m_ovl_front.load(std::memory_order_relaxed);
    const int back = (front == 0) ? 1 : 0;

    m_ovl_rt_cmdmem->begin(m_ovl_rt_cmdbuf);
    dk::ImageView rtView{m_ovl_rt_image[back]};
    dk::ImageView dsView{m_ovl_ds_image};
    m_ovl_rt_cmdbuf.bindRenderTargets(&rtView, &dsView);
    m_ovl_rt_cmdbuf.setViewports(0, {{ 0.0f, 0.0f, (float)m_ovl_rt_w, (float)m_ovl_rt_h, 0.0f, 1.0f }});
    m_ovl_rt_cmdbuf.setScissors(0, {{ 0, 0, m_ovl_rt_w, m_ovl_rt_h }});
    m_ovl_rt_cmdbuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 0.0f);
    m_ovl_rt_cmdbuf.clearDepthStencil(true, 1.0f, 0xFF, 0);
    DkCmdList prep = m_ovl_rt_cmdmem->end(m_ovl_rt_cmdbuf);
    if (prep)
        m_ovl_queue.submitCommands(prep);

    m_overlay_to_texture = true;
    nvgBeginFrame(m_ovl_vg, (float)m_ovl_rt_w, (float)m_ovl_rt_h, 1.0f);
    nvgFontFaceId(m_ovl_vg, m_overlay_font);
    warmFontAtlas(m_ovl_vg);

    if (m_stats_mode.load(std::memory_order_relaxed) == StatsOverlayMode::Compact)
        drawCompactOverlay(m_ovl_vg);
    else
        drawFullOverlay(m_ovl_vg);

    nvgEndFrame(m_ovl_vg);
    m_overlay_to_texture = false;

    m_ovl_queue.flush();
    m_ovl_queue.waitIdle();

    m_ovl_panel_w.store(m_overlay_w.load(std::memory_order_relaxed), std::memory_order_relaxed);
    m_ovl_panel_h.store(m_overlay_h.load(std::memory_order_relaxed), std::memory_order_relaxed);
    m_ovl_dirty = false;
    m_ovl_front.store(back, std::memory_order_release);
}

bool Deko3dRenderer::ensureOverlayShaders()
{
    if (m_ovl_shaders_ready)
        return true;

    if (!m_pool_data)
        return false;

    std::string vsh = loadShaderSource("romfs:/shaders/overlay_vsh.glsl");
    std::string fsh = loadShaderSource("romfs:/shaders/overlay_fsh.glsl");
    std::string bsh = loadShaderSource("romfs:/shaders/border_fsh.glsl");
    if (vsh.empty() || fsh.empty() || bsh.empty())
    {
        brls::Logger::error("overlay: blit shader sources missing");
        return false;
    }
    if (!compileShaderFromSource(m_ovl_vertex_shader, vsh, true) ||
        !compileShaderFromSource(m_ovl_fragment_shader, fsh, false) ||
        !compileShaderFromSource(m_border_fragment_shader, bsh, false))
    {
        brls::Logger::error("overlay: blit shader compilation failed");
        return false;
    }

    m_ovl_cmdbuf = dk::CmdBufMaker{m_device}.create();
    m_ovl_cmdmem.emplace();
    m_ovl_cmdmem->allocate(*m_pool_data, 8 * 1024);
    m_ovl_uniform = m_pool_data->allocate(6 * 256, DK_UNIFORM_BUF_ALIGNMENT);

    m_ovl_desc_cmdbuf = dk::CmdBufMaker{m_device}.create();
    m_ovl_desc_cmdmem = m_pool_data->allocate(0x1000, DK_CMDMEM_ALIGNMENT);

    m_ovl_shaders_ready = true;
    return true;
}

bool Deko3dRenderer::ensureOverlayContext()
{
    if (m_ovl_ctx_ready)
        return true;

    m_ovl_queue = dk::QueueMaker{m_device}
        .setFlags(DkQueueFlags_Graphics | DkQueueFlags_DisableZcull)
        .create();

    m_ovl_images_pool.emplace(m_device, DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image, 8 * 1024 * 1024);
    m_ovl_code_pool.emplace(m_device, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code, 128 * 1024);
    m_ovl_data_pool.emplace(m_device, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, 2 * 1024 * 1024);

    m_ovl_renderer.emplace(1024u, 512u, m_device, dk::Queue{m_ovl_queue},
                           *m_ovl_images_pool, *m_ovl_code_pool, *m_ovl_data_pool);

    m_ovl_vg = nvgCreateDk(&*m_ovl_renderer, NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!m_ovl_vg)
    {
        brls::Logger::error("overlay: private nanovg context creation failed");
        return false;
    }

    m_overlay_font = nvgCreateFont(m_ovl_vg, "mono", BRLS_ASSET("font/Cousine-Regular.ttf"));
    if (m_overlay_font < 0)
    {
        brls::Logger::error("overlay: private context could not load the mono font");
        return false;
    }

    m_ovl_rt_cmdbuf = dk::CmdBufMaker{m_device}.create();
    m_ovl_rt_cmdmem.emplace();
    m_ovl_rt_cmdmem->allocate(*m_ovl_data_pool, 8 * 1024);

    m_ovl_ctx_ready = true;
    brls::Logger::info("overlay: private queue and nanovg context ready");
    return true;
}

void Deko3dRenderer::destroyOverlayContext()
{
    if (!m_ovl_ctx_ready)
        return;

    m_ovl_queue.waitIdle();

    if (m_ovl_vg)
    {
        nvgDeleteDk(m_ovl_vg);
        m_ovl_vg = nullptr;
    }
    m_ovl_renderer.reset();
    m_ovl_rt_cmdmem.reset();
    m_ovl_rt_cmdbuf = nullptr;
    m_ovl_queue = nullptr;
    m_ovl_images_pool.reset();
    m_ovl_code_pool.reset();
    m_ovl_data_pool.reset();
    m_overlay_font = -1;
    m_ovl_ctx_ready = false;
}

bool Deko3dRenderer::ensureOverlayTarget()
{
    const float s = m_overlay_scale;
    const unsigned wantW = (unsigned)(620.0f * s) + 4;
    const unsigned wantH = (unsigned)(240.0f * s) + 4;

    if (m_ovl_rt_texture_id[0] && m_ovl_rt_w == wantW && m_ovl_rt_h == wantH)
        return true;

    if (!ensureOverlayShaders())
        return false;
    if (!ensureOverlayContext())
        return false;

    destroyOverlayTarget();

    dk::ImageLayoutMaker{m_device}
        .setType(DkImageType_2D)
        .setFormat(DkImageFormat_RGBA8_Unorm)
        .setDimensions(wantW, wantH, 1)
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_UsageLoadStore | DkImageFlags_Usage2DEngine)
        .initialize(m_ovl_rt_layout);

    dk::ImageLayoutMaker{m_device}
        .setType(DkImageType_2D)
        .setFormat(DkImageFormat_S8)
        .setDimensions(wantW, wantH, 1)
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_HwCompression)
        .initialize(m_ovl_ds_layout);

    m_ovl_ds_handle = m_ovl_images_pool->allocate(m_ovl_ds_layout.getSize(), m_ovl_ds_layout.getAlignment());
    if (!m_ovl_ds_handle)
    {
        brls::Logger::error("overlay: could not allocate depth target");
        return false;
    }
    m_ovl_ds_image.initialize(m_ovl_ds_layout, m_ovl_ds_handle.getMemBlock(), m_ovl_ds_handle.getOffset());

    m_ovl_desc_cmdbuf.clear();
    m_ovl_desc_cmdbuf.addMemory(m_ovl_desc_cmdmem.getMemBlock(), m_ovl_desc_cmdmem.getOffset(), m_ovl_desc_cmdmem.getSize());

    for (int i = 0; i < 2; ++i)
    {
        m_ovl_rt_handle[i] = m_ovl_images_pool->allocate(m_ovl_rt_layout.getSize(), m_ovl_rt_layout.getAlignment());
        if (!m_ovl_rt_handle[i])
        {
            brls::Logger::error("overlay: could not allocate {}x{} render target", wantW, wantH);
            destroyOverlayTarget();
            return false;
        }
        m_ovl_rt_image[i].initialize(m_ovl_rt_layout, m_ovl_rt_handle[i].getMemBlock(), m_ovl_rt_handle[i].getOffset());
        m_ovl_rt_desc[i].initialize(m_ovl_rt_image[i], true);
        m_ovl_rt_texture_id[i] = m_vctx->allocateImageIndex();

        if (!m_vctx->updateImageDescriptor(m_ovl_desc_cmdbuf, m_ovl_rt_texture_id[i], m_ovl_rt_desc[i]))
        {
            brls::Logger::error("overlay: failed to bind render target descriptor");
            destroyOverlayTarget();
            return false;
        }
    }

    m_vctx->invalidateImageDescriptors(m_ovl_desc_cmdbuf);
    m_ovl_desc_list = m_ovl_desc_cmdbuf.finishList();
    m_ovl_desc_live = false;
    m_ovl_desc_pending.store(true, std::memory_order_release);

    m_ovl_renderer->UpdateViewBounds(wantW, wantH);

    m_ovl_rt_w = wantW;
    m_ovl_rt_h = wantH;
    m_ovl_rt_scale = s;
    m_ovl_dirty = true;

    brls::Logger::info("overlay: render targets {}x{} ready (tex={},{})",
                       wantW, wantH, m_ovl_rt_texture_id[0], m_ovl_rt_texture_id[1]);
    return true;
}

void Deko3dRenderer::destroyOverlayTarget()
{
    m_ovl_front.store(-1, std::memory_order_release);
    m_ovl_desc_pending.store(false, std::memory_order_release);
    m_ovl_desc_live = false;

    if (m_ovl_ctx_ready)
        m_ovl_queue.waitIdle();

    for (int i = 0; i < 2; ++i)
    {
        if (m_ovl_rt_texture_id[i])
        {
            m_vctx->freeImageIndex(m_ovl_rt_texture_id[i]);
            m_ovl_rt_texture_id[i] = 0;
        }
        m_ovl_rt_handle[i].destroy();
        m_ovl_rt_image[i] = dk::Image{};
    }
    m_ovl_ds_handle.destroy();
    m_ovl_ds_image = dk::Image{};
    m_ovl_rt_w = 0;
    m_ovl_rt_h = 0;
}

void Deko3dRenderer::compositeOverlay()
{
    if (m_ovl_desc_pending.exchange(false, std::memory_order_acquire))
    {
        if (m_ovl_desc_list)
            m_queue.submitCommands(m_ovl_desc_list);
        m_ovl_desc_live = true;
    }

    if (!m_ovl_desc_live)
        return;

    const int front = m_ovl_front.load(std::memory_order_acquire);
    if (front < 0 || !m_ovl_rt_texture_id[front])
        return;

    dk::Image* framebuffer = m_vctx->getFramebuffer();
    if (!framebuffer)
        return;

    const float sw = (float)brls::Application::windowWidth;
    const float sh = (float)brls::Application::windowHeight;
    if (sw <= 0.0f || sh <= 0.0f)
        return;

    const float panelW = m_ovl_panel_w.load(std::memory_order_relaxed);
    const float panelH = m_ovl_panel_h.load(std::memory_order_relaxed);
    if (panelW <= 0.0f || panelH <= 0.0f)
        return;

    float ox = m_overlay_x.load(std::memory_order_relaxed);
    float oy = m_overlay_y.load(std::memory_order_relaxed);

    float uniforms[8] = {
        (ox / sw) * 2.0f - 1.0f,
        1.0f - (oy / sh) * 2.0f,
        ((ox + panelW) / sw) * 2.0f - 1.0f,
        1.0f - ((oy + panelH) / sh) * 2.0f,
        panelW / (float)m_ovl_rt_w,
        panelH / (float)m_ovl_rt_h,
        0.0f,
        0.0f,
    };

    m_ovl_uniform_slot ^= 1u;
    const unsigned off = m_ovl_uniform_slot * 256;
    memcpy((uint8_t*)m_ovl_uniform.getCpuAddr() + off, uniforms, sizeof(uniforms));

    m_ovl_cmdmem->begin(m_ovl_cmdbuf);

    dk::ImageView colorTarget{*framebuffer};
    m_ovl_cmdbuf.bindRenderTargets(&colorTarget);
    m_ovl_cmdbuf.setViewports(0, {{ 0.0f, 0.0f, sw, sh, 0.0f, 1.0f }});
    m_ovl_cmdbuf.setScissors(0, {{ 0, 0, (uint32_t)sw, (uint32_t)sh }});
    m_ovl_cmdbuf.bindRasterizerState(dk::RasterizerState{}.setCullMode(DkFace_None));
    m_ovl_cmdbuf.bindDepthStencilState(dk::DepthStencilState{}
        .setDepthTestEnable(false)
        .setDepthWriteEnable(false)
        .setStencilTestEnable(false));
    m_ovl_cmdbuf.bindColorState(dk::ColorState{}.setBlendEnable(0, true));
    m_ovl_cmdbuf.bindBlendStates(0, dk::BlendState{}.setFactors(
        DkBlendFactor_One, DkBlendFactor_InvSrcAlpha,
        DkBlendFactor_One, DkBlendFactor_InvSrcAlpha));
    m_ovl_cmdbuf.bindColorWriteState(dk::ColorWriteState{});

    m_ovl_cmdbuf.bindShaders(DkStageFlag_GraphicsMask, { m_ovl_vertex_shader, m_ovl_fragment_shader });
    m_ovl_cmdbuf.bindUniformBuffer(DkStage_Vertex, 0, m_ovl_uniform.getGpuAddr() + off, 256);
    m_ovl_cmdbuf.bindTextures(DkStage_Fragment, 0, dkMakeTextureHandle(m_ovl_rt_texture_id[front], 0));
    m_ovl_cmdbuf.bindVtxBuffer(0, m_vertex_buffer.getGpuAddr(), m_vertex_buffer.getSize());
    m_ovl_cmdbuf.bindVtxAttribState(VertexAttribState);
    m_ovl_cmdbuf.bindVtxBufferState(VertexBufferState);
    m_ovl_cmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);

    DkCmdList list = m_ovl_cmdmem->end(m_ovl_cmdbuf);
    if (list)
        m_queue.submitCommands(list);
    m_queue.flush();
}

void Deko3dRenderer::renderBorderFlash()
{
    int before = m_border_flash_frames.load(std::memory_order_acquire);
    while (before > 0
           && !m_border_flash_frames.compare_exchange_weak(
               before, before - 1, std::memory_order_acq_rel)) {
    }
    if (before <= 0)
        return;
    const int remaining = before - 1;

    if (!m_ovl_shaders_ready)
        return;

    dk::Image* framebuffer = m_vctx->getFramebuffer();
    if (!framebuffer)
        return;

    const float sw = (float)brls::Application::windowWidth;
    const float sh = (float)brls::Application::windowHeight;
    if (sw <= 0.0f || sh <= 0.0f)
        return;

    const float fade = (float)remaining / (float)BORDER_FLASH_DURATION;
    const float alpha = fade * 0.25f;
    if (alpha <= 0.002f)
        return;

    NVGcolor accent = ovl(akira::ui::active().accent, 255);

    float uniforms[8] = {
        accent.r, accent.g, accent.b, alpha,
        sw, sh, 64.0f, 0.0f,
    };

    m_border_uniform_slot ^= 1u;
    const unsigned off = (3 + m_border_uniform_slot) * 256;
    memcpy((uint8_t*)m_ovl_uniform.getCpuAddr() + off, uniforms, sizeof(uniforms));

    float rect[8] = { -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f };
    memcpy((uint8_t*)m_ovl_uniform.getCpuAddr() + 2 * 256, rect, sizeof(rect));

    m_ovl_cmdmem->begin(m_ovl_cmdbuf);

    dk::ImageView colorTarget{*framebuffer};
    m_ovl_cmdbuf.bindRenderTargets(&colorTarget);
    m_ovl_cmdbuf.setViewports(0, {{ 0.0f, 0.0f, sw, sh, 0.0f, 1.0f }});
    m_ovl_cmdbuf.setScissors(0, {{ 0, 0, (uint32_t)sw, (uint32_t)sh }});
    m_ovl_cmdbuf.bindRasterizerState(dk::RasterizerState{}.setCullMode(DkFace_None));
    m_ovl_cmdbuf.bindDepthStencilState(dk::DepthStencilState{}
        .setDepthTestEnable(false)
        .setDepthWriteEnable(false)
        .setStencilTestEnable(false));
    m_ovl_cmdbuf.bindColorState(dk::ColorState{}.setBlendEnable(0, true));
    m_ovl_cmdbuf.bindBlendStates(0, dk::BlendState{}.setFactors(
        DkBlendFactor_One, DkBlendFactor_InvSrcAlpha,
        DkBlendFactor_One, DkBlendFactor_InvSrcAlpha));
    m_ovl_cmdbuf.bindColorWriteState(dk::ColorWriteState{});

    m_ovl_cmdbuf.bindShaders(DkStageFlag_GraphicsMask, { m_ovl_vertex_shader, m_border_fragment_shader });
    m_ovl_cmdbuf.bindUniformBuffer(DkStage_Vertex, 0, m_ovl_uniform.getGpuAddr() + 2 * 256, 256);
    m_ovl_cmdbuf.bindUniformBuffer(DkStage_Fragment, 0, m_ovl_uniform.getGpuAddr() + off, 256);
    m_ovl_cmdbuf.bindVtxBuffer(0, m_vertex_buffer.getGpuAddr(), m_vertex_buffer.getSize());
    m_ovl_cmdbuf.bindVtxAttribState(VertexAttribState);
    m_ovl_cmdbuf.bindVtxBufferState(VertexBufferState);
    m_ovl_cmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);

    DkCmdList list = m_ovl_cmdmem->end(m_ovl_cmdbuf);
    if (list)
        m_queue.submitCommands(list);
    m_queue.flush();
}

bool Deko3dRenderer::captureLastFrame(std::vector<uint8_t>& rgba, int& width, int& height)
{
    if (!m_initialized)
        return false;

    using Frame = std::unique_ptr<AVFrame, decltype([](AVFrame* frame) { av_frame_free(&frame); })>;
    Frame held;
    {
        std::lock_guard lock(m_frame_ring_mutex);
        const int newest = (m_frame_ring_index - 1 + FRAME_RING_SIZE) % FRAME_RING_SIZE;
        if (m_frame_ring[newest])
            held.reset(av_frame_clone(m_frame_ring[newest]));
    }
    AVFrame* src = held.get();
    if (!src || src->width <= 0 || src->height <= 0)
        return false;

    AVFrame* sw = av_frame_alloc();
    if (!sw)
        return false;

    bool owns_sw = true;
    if (src->hw_frames_ctx)
    {
        const auto* frames = reinterpret_cast<const AVHWFramesContext*>(src->hw_frames_ctx->data);
        sw->format = frames->sw_format;
        sw->width = src->width;
        sw->height = src->height;
        if (av_frame_get_buffer(sw, HW_TRANSFER_ALIGNMENT) < 0 || av_hwframe_transfer_data(sw, src, 0) < 0)
        {
            av_frame_free(&sw);
            return false;
        }
    }
    else
    {
        av_frame_free(&sw);
        sw = src;
        owns_sw = false;
    }

    int dstW = CAPTURE_WIDTH;
    int dstH = (int)((int64_t)dstW * src->height / src->width) & ~1;
    if (dstH <= 0)
        dstH = CAPTURE_WIDTH * 9 / 16;

    SwsContext* sws = sws_getContext(
        sw->width, sw->height, (AVPixelFormat)sw->format,
        dstW, dstH, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws)
    {
        if (owns_sw) av_frame_free(&sw);
        return false;
    }

    rgba.assign((size_t)dstW * dstH * 4, 0);
    uint8_t* dstData[4] = { rgba.data(), nullptr, nullptr, nullptr };
    int dstLinesize[4] = { dstW * 4, 0, 0, 0 };

    int scaled = sws_scale(sws, sw->data, sw->linesize, 0, sw->height, dstData, dstLinesize);
    sws_freeContext(sws);
    if (owns_sw)
        av_frame_free(&sw);

    if (scaled <= 0)
    {
        rgba.clear();
        return false;
    }

    width = dstW;
    height = dstH;
    return true;
}

#endif // BOREALIS_USE_DEKO3D
