#include "rhiitem.h"

#include <rhi/qrhi.h>
#include <rhi/qshader.h>

#include <QFile>
#include <QImage>

using namespace Qt::StringLiterals;

// see https://github.com/libretro/slang-shaders/tree/master?tab=readme-ov-file#builtin-texture-size-uniform-variables
QVector4D asVec4d(QSize sz)
{
    return {float(sz.width()), float(sz.height()), 1.0f / sz.width(), 1.0f / sz.height()};
}

class ItemRenderer : public QQuickRhiItemRenderer
{
public:
    ItemRenderer();
    // QQuickRhiItemRenderer interface
protected:
    void initialize(QRhiCommandBuffer *cb) final;
    void synchronize(QQuickRhiItem *item) final;
    void render(QRhiCommandBuffer *cb) final;

private:
    static QShader getShader(const QString &name)
    {
        QFile f(name);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open shader file:" << name;
            return {};
        }
        auto shader = QShader::fromSerialized(f.readAll());
        auto shaders = shader.availableShaders();
        fprintf(stdout, "%s\n\n", name.toUtf8().constData());
        fprintf(stdout, "%s", shader.description().toJson().constData());
        fprintf(stdout, "%s", shader.shader(shaders[2]).shader().constData());
        fflush(stdout);
#warning "Check the output, you'll see that `uniform Push params;` and `uniform UBO global;` are swapped :("
/*
        :/simple.vert.qsb
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
    .......


        :/simple.frag.qsb
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


        :/simple.frag.qsb has params & global uniforms swapped ?!?!
*/
        return shader;
    }

private:
    QImage m_img;
    QRhi *m_rhi = nullptr;
    struct {
        QVector4D SourceSize;
        QVector4D OriginalSize;
        QVector4D OutputSize;
        uint FrameCount = 0;
        float BIL_FALLBACK = 0.6;
        char padding[8];
    } pushData;
    static_assert(sizeof(pushData) == 64, "Invalid pushData size");

    QRhiTexture::Format m_textureFormat = QRhiTexture::RGBA8;
    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline;
    std::unique_ptr<QRhiBuffer> m_vertexBuffer;
    std::unique_ptr<QRhiBuffer> m_uniformBuffer;
    std::unique_ptr<QRhiBuffer> m_pushConstantsBuffer;
    std::unique_ptr<QRhiTexture> m_texture;
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiShaderResourceBindings> m_srb;

};

ItemRenderer::ItemRenderer()
{
    m_img.load(u":/images/dizzy_4.png"_s);
    m_img.convertTo(QImage::Format_RGBA8888);
}

void ItemRenderer::initialize(QRhiCommandBuffer *cb)
{
    if (rhi() == m_rhi)
        return;
    m_rhi = rhi();

    m_pipeline.reset();
    m_texture.reset(m_rhi->newTexture(QRhiTexture::RGBA8, m_img.size()));
    m_texture->create();

    static const float vertices[] = {
        // Triangle strip that makes 2 CCW triangles
        // position (vec4)           // texcoord
        -1.0f,  1.0f, 0.0f, 1.0f,     0.0f, 0.0f, // Top-left
        -1.0f, -1.0f, 0.0f, 1.0f,     0.0f, 1.0f, // Bottom-left
        1.0f,  1.0f, 0.0f, 1.0f,     1.0f, 0.0f, // Top-right
        1.0f, -1.0f, 0.0f, 1.0f,     1.0f, 1.0f  // Bottom-right
    };

    pushData.BIL_FALLBACK = 0.6f;
    pushData.FrameCount = 0;
    pushData.OriginalSize = asVec4d(m_img.size());
    pushData.SourceSize = pushData.OriginalSize;

    m_vertexBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertices)));
    m_vertexBuffer->create();
    auto resourceUpdates = m_rhi->nextResourceUpdateBatch();
    resourceUpdates->uploadStaticBuffer(m_vertexBuffer.get(), vertices);
    resourceUpdates->uploadTexture(m_texture.get(), m_img);


    m_uniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
    m_uniformBuffer->create();

    m_pushConstantsBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(pushData)));
    m_pushConstantsBuffer->create();

    m_sampler.reset(m_rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                      QRhiSampler::Mirror, QRhiSampler::Mirror));
    m_sampler->create();

    m_srb.reset(m_rhi->newShaderResourceBindings());
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_uniformBuffer.get()),
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_pushConstantsBuffer.get()),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, m_texture.get(), m_sampler.get()),
    });
    m_srb->create();


    m_pipeline.reset(m_rhi->newGraphicsPipeline());
    m_pipeline->setShaderStages({
        // I used https://github.com/libretro/slang-shaders/blob/master/edge-smoothing/ddt/shaders/ddt.slang as the test shader
        { QRhiShaderStage::Vertex, getShader(QLatin1String(":/simple.vert.qsb")) },
        { QRhiShaderStage::Fragment, getShader(QLatin1String(":/simple.frag.qsb")) }
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({ QRhiVertexInputBinding(6 * sizeof(float)) });
    inputLayout.setAttributes({
        QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float4, 0),
        QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2, 4 * sizeof(float))
    });
    m_pipeline->setVertexInputLayout(inputLayout);

    m_pipeline->setShaderResourceBindings(m_srb.get());
    m_pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    m_pipeline->setCullMode(QRhiGraphicsPipeline::None);
    m_pipeline->setFrontFace(QRhiGraphicsPipeline::CCW);
    m_pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_pipeline->create();
    cb->resourceUpdate(resourceUpdates);
}

void ItemRenderer::synchronize(QQuickRhiItem *item)
{
}

void ItemRenderer::render(QRhiCommandBuffer *cb)
{
    const QSize outputSize = renderTarget()->pixelSize();
    pushData.OutputSize = asVec4d(outputSize);

    QMatrix4x4 modelViewProjection = m_rhi->clipSpaceCorrMatrix();
    auto resourceUpdates = m_rhi->nextResourceUpdateBatch();
//    static_assert(sizeof(QMatrix4x4) == 64, "Invalid QMatrix4x4 size"); // actually sizeof(QMatrix4x4) == 68 because of the flagBits member
    resourceUpdates->updateDynamicBuffer(m_uniformBuffer.get(), 0, 64, modelViewProjection.constData());
    resourceUpdates->updateDynamicBuffer(m_pushConstantsBuffer.get(), 0, sizeof(pushData), &pushData);

    const QColor clearColor = QColor::fromRgbF(1, 1, 1, 1);
    cb->beginPass(renderTarget(), clearColor, { 1.0f, 0 }, resourceUpdates);
    cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));
    cb->setGraphicsPipeline(m_pipeline.get());
    cb->setShaderResources();
    const QRhiCommandBuffer::VertexInput vbufBinding(m_vertexBuffer.get(), 0);
    cb->setVertexInput(0, 1, &vbufBinding);
    cb->draw(4);
    cb->endPass();
}

RhiItem::RhiItem(QQuickItem *parent)
    : QQuickRhiItem(parent)
{
    setFlags(ItemHasContents | ItemIsFocusScope);
    setFocus(true);
    setWidth(320 * 3);
    setHeight(240 * 3);
}

QQuickRhiItemRenderer *RhiItem::createRenderer()
{
    return new ItemRenderer;
}
