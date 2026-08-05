#[compute]

#version 450

#VERSION_DEFINES

// Nanite Stage 1 composite shader (Task 1.18 / 差距 1 修复).
//
// 把 material_resolve 写入的内部 color_buffer 合成到引擎 color target,
// 仅覆盖 vis_buffer != 0 的像素(有 Nanite 几何的像素),保留引擎背景。
//
// 这是 Stage 1 "将模型渲染到场景中" 的最后一公里:material_resolve 产出
// 着色后的 color_buffer,但写入的是 pipeline 内部纹理;本 shader 把它
// 合成到 RenderSceneBuffersRD::get_color_layer(0) 返回的引擎 color target。
//
// Bindings:
//   0: vis_buffer (r32ui, readonly) — mask: 非 0 表示该像素有 Nanite 几何
//   1: color_buffer (rgba8, readonly) — material_resolve 的着色输出
//   2: engine_color_target (rgba8, writeonly) — 引擎 color target (来自 RenderSceneBuffersRD)
//
// 注意:引擎 color target 是 RGBA8 UNORM(与内部 color_buffer 一致),
// 不需要格式转换。如果未来引擎 target 格式变化,需要在此处理。
//
// RT 反转:Godot 的 RenderTarget 纹理在 Vulkan 下 Y 轴已经是正确的方向
// (引擎在 create_texture 时处理了翻转),本 shader 直接按 pos 读写即可。

layout(set = 0, binding = 0, r32ui) uniform readonly uimage2D vis_buffer;
layout(set = 0, binding = 1, rgba8) uniform readonly image2D color_buffer;
// engine_color_target 不限定格式(writeonly image2D),让 imageStore 使用
// 纹理实际格式。Godot 4.x Forward+ 的 color buffer 是 RGBA16F(HDR),
// 如果声明为 rgba8 会导致格式不匹配,imageStore 行为未定义。
layout(set = 0, binding = 2) uniform writeonly image2D engine_color_target;

layout(push_constant, std430) uniform Params {
	ivec2 screen_size;
	uint _pad0;
	uint _pad1;
} params;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main() {
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (pos.x >= params.screen_size.x || pos.y >= params.screen_size.y) {
		return;
	}

	// 读取 vis_buffer 作为 mask:0 表示该像素没有 Nanite 几何,跳过
	// (保留引擎已渲染的背景)。
	uint encoded = imageLoad(vis_buffer, pos).r;
	if (encoded == 0u) {
		return;
	}

	// 该像素有 Nanite 几何,从内部 color_buffer 读取着色结果,写入引擎 target。
	vec4 color = imageLoad(color_buffer, pos);
	imageStore(engine_color_target, pos, color);
}
