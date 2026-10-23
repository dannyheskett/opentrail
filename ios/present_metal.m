// Metal, for one thing only: the game's canvas (an RGBA buffer, 384-480 x
// 252, drawn by src/modern/draw.c) uploaded as a texture each frame and drawn
// as one quad, sampled nearest, into the safe rectangle; black elsewhere.
// The shader is compiled from source at run time (no offline Metal compiler).
// Only the game thread calls ios_present(), after ios_metal_attach().
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "plat_ios.h"

static CAMetalLayer *s_layer;
static id<MTLDevice> s_device;
static id<MTLCommandQueue> s_queue;
static id<MTLRenderPipelineState> s_pipeline;
static id<MTLSamplerState> s_sampler;
static id<MTLTexture> s_tex;
static int s_tw, s_th;

static NSString *const kShader = @
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct VOut { float4 pos [[position]]; float2 uv; };\n"
    "vertex VOut v_main(uint vid [[vertex_id]], constant float4 &q [[buffer(0)]]) {\n"
    "    VOut o;\n"
    "    o.pos = float4((vid & 1) ? q.z : q.x, (vid & 2) ? q.w : q.y, 0.0, 1.0);\n"
    "    o.uv = float2((vid & 1) ? 1.0 : 0.0, (vid & 2) ? 1.0 : 0.0);\n"
    "    return o;\n"
    "}\n"
    "fragment float4 f_main(VOut in [[stage_in]], texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {\n"
    "    return float4(t.sample(s, in.uv).rgb, 1.0);\n"
    "}\n";

void ios_metal_attach(CAMetalLayer *layer) {
    s_layer = layer;
    s_device = MTLCreateSystemDefaultDevice();
    s_layer.device = s_device;
    s_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    s_queue = [s_device newCommandQueue];
    NSError *err = nil;
    id<MTLLibrary> lib = [s_device newLibraryWithSource:kShader options:nil error:&err];
    if (!lib) { NSLog(@"opentrail: Metal shader: %@", err); return; }
    MTLRenderPipelineDescriptor *pd = [[MTLRenderPipelineDescriptor alloc] init];
    pd.vertexFunction = [lib newFunctionWithName:@"v_main"];
    pd.fragmentFunction = [lib newFunctionWithName:@"f_main"];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    s_pipeline = [s_device newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!s_pipeline) NSLog(@"opentrail: Metal pipeline: %@", err);
    MTLSamplerDescriptor *sd = [[MTLSamplerDescriptor alloc] init];
    sd.minFilter = MTLSamplerMinMagFilterNearest;
    sd.magFilter = MTLSamplerMinMagFilterNearest;
    sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
    s_sampler = [s_device newSamplerStateWithDescriptor:sd];
}

void ios_present(const uint8_t *rgba, int cw, int ch, float x, float y, float w, float h) {
    if (!s_pipeline) return;
    @autoreleasepool {
        if (!s_tex || s_tw != cw || s_th != ch) {
            MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                          width:cw height:ch mipmapped:NO];
            td.usage = MTLTextureUsageShaderRead;
            s_tex = [s_device newTextureWithDescriptor:td];
            s_tw = cw; s_th = ch;
        }
        [s_tex replaceRegion:MTLRegionMake2D(0, 0, cw, ch) mipmapLevel:0 withBytes:rgba bytesPerRow:(NSUInteger)cw * 4];
        id<CAMetalDrawable> drawable = [s_layer nextDrawable];
        if (!drawable) return;
        CGSize ds = s_layer.drawableSize;
        float W = (float)ds.width, H = (float)ds.height;
        float q[4] = { x / W * 2 - 1, 1 - y / H * 2, (x + w) / W * 2 - 1, 1 - (y + h) / H * 2 };
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = drawable.texture;
        rp.colorAttachments[0].loadAction = MTLLoadActionClear;
        rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLCommandBuffer> cb = [s_queue commandBuffer];
        id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
        [enc setRenderPipelineState:s_pipeline];
        [enc setVertexBytes:q length:sizeof q atIndex:0];
        [enc setFragmentTexture:s_tex atIndex:0];
        [enc setFragmentSamplerState:s_sampler atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
        [enc endEncoding];
        [cb presentDrawable:drawable];
        [cb commit];
        [cb waitUntilCompleted];                    // the texture is written again next frame
    }
}
