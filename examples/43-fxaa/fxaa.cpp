/*
 * Copyright 2016 Joseph Cherlin. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include <common.h>
#include <camera.h>
#include <bgfx_utils.h>
#include <imgui/imgui.h>
#include <bx/rng.h>

namespace
{

/*
 * Intro
 * =====
 *
 *
 * Overview:
 *
 *
 * Details
 * =======
 *
 *
 * References
 * ==========
 *
 *
 */

// Render passes
#define RENDER_PASS_ALBEDO 0  // [todo]
#define RENDER_PASS_FXAA   1  // [todo]
#define RENDER_PASS_BLIT   2  // [todo]

// Vertex layout for our screen space quad (used in deferred rendering)
struct PosTexCoord0Vertex
{
	float m_x;
	float m_y;
	float m_z;
	float m_u;
	float m_v;

	static void init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.end();
	}

	static bgfx::VertexLayout ms_layout;
};
bgfx::VertexLayout PosTexCoord0Vertex::ms_layout;

// Utility function to draw a screen space quad for deferred rendering
void screenSpaceQuad(float _textureWidth, float _textureHeight, float _texelHalf, bool _originBottomLeft, float _width = 1.0f, float _height = 1.0f)
{
	if (3 == bgfx::getAvailTransientVertexBuffer(3, PosTexCoord0Vertex::ms_layout) )
	{
		bgfx::TransientVertexBuffer vb;
		bgfx::allocTransientVertexBuffer(&vb, 3, PosTexCoord0Vertex::ms_layout);
		PosTexCoord0Vertex* vertex = (PosTexCoord0Vertex*)vb.data;

		const float minx = -_width;
		const float maxx =  _width;
		const float miny = 0.0f;
		const float maxy = _height*2.0f;

		const float texelHalfW = _texelHalf/_textureWidth;
		const float texelHalfH = _texelHalf/_textureHeight;
		const float minu = -1.0f + texelHalfW;
		const float maxu =  1.0f + texelHalfH;

		const float zz = 0.0f;

		float minv = texelHalfH;
		float maxv = 2.0f + texelHalfH;

		if (_originBottomLeft)
		{
			float temp = minv;
			minv = maxv;
			maxv = temp;

			minv -= 1.0f;
			maxv -= 1.0f;
		}

		vertex[0].m_x = minx;
		vertex[0].m_y = miny;
		vertex[0].m_z = zz;
		vertex[0].m_u = minu;
		vertex[0].m_v = minv;

		vertex[1].m_x = maxx;
		vertex[1].m_y = miny;
		vertex[1].m_z = zz;
		vertex[1].m_u = maxu;
		vertex[1].m_v = minv;

		vertex[2].m_x = maxx;
		vertex[2].m_y = maxy;
		vertex[2].m_z = zz;
		vertex[2].m_u = maxu;
		vertex[2].m_v = maxv;

		bgfx::setVertexBuffer(0, &vb);
	}
}

class ExampleFXAA : public entry::AppI
{
public:
	ExampleFXAA(const char* _name, const char* _description, const char* _url)
		: entry::AppI(_name, _description, _url)
		, m_reading(0)
		, m_currFrame(UINT32_MAX)
	{
	}

	void init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height) override
	{
		Args args(_argc, _argv);

		m_width  = _width;
		m_height = _height;
		m_debug  = BGFX_DEBUG_NONE;
		m_reset  = BGFX_RESET_VSYNC;

		bgfx::Init init;
		init.type     = args.m_type;
		init.vendorId = args.m_pciId;
		init.resolution.width  = m_width;
		init.resolution.height = m_height;
		init.resolution.reset  = m_reset;
		bgfx::init(init);

		// Enable debug text.
		bgfx::setDebug(m_debug);

		// Labeling for renderdoc captures, etc
		bgfx::setViewName(RENDER_PASS_ALBEDO,  "[todo]");
		bgfx::setViewName(RENDER_PASS_FXAA,    "[todo]");
		bgfx::setViewName(RENDER_PASS_BLIT,    "[todo]");

		// Set up screen clears
		bgfx::setViewClear(RENDER_PASS_ALBEDO
			, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
			, 0x303030ff
			, 1.0f
			, 0
			);

		bgfx::setViewClear(RENDER_PASS_FXAA
			, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
			, 0
			, 1.0f
			, 0
			);

		bgfx::setViewClear(RENDER_PASS_BLIT
			, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
			, 0
			, 1.0f
			, 0
			);

		// Create uniforms
		u_tint       = bgfx::createUniform("u_tint",          bgfx::UniformType::Vec4);  // Tint for when you click on items
		u_fxaaParams = bgfx::createUniform("u_fxaaParams",    bgfx::UniformType::Vec4);  // FXAA parameters

		// Create texture sampler uniforms (used when we bind textures)
		s_albedo = bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler);  // Color (albedo) gbuffer
		s_source = bgfx::createUniform("s_source", bgfx::UniformType::Sampler);

		// Create program from shaders.
		m_albedoProgram = loadProgram("vs_fxaa_albedo", "fs_fxaa_albedo");    // Draw scene
		m_fxaaProgram   = loadProgram("vs_fxaa_apply",  "fs_fxaa_apply" );    // FXAA
		m_blitProgram   = loadProgram("vs_fxaa_blit",   "fs_fxaa_blit"  );    // Blit

		// Load some meshes
		m_mesh = meshLoad("meshes/hollowcube.bin");

		const uint64_t tsFlags = 0
			| BGFX_TEXTURE_RT
			| BGFX_SAMPLER_U_CLAMP
			| BGFX_SAMPLER_V_CLAMP
			;

		// Make gbuffer and related textures
		bgfx::TextureFormat::Enum depthFormat = bgfx::getRendererType() == bgfx::RendererType::WebGPU
			? bgfx::TextureFormat::D32F
			: bgfx::TextureFormat::D24;

		bgfx::TextureHandle gbufferTex[] =
		{
			bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, false, 1, bgfx::TextureFormat::RGBA8, tsFlags),
			bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, false, 1, depthFormat,                tsFlags)
		};

		m_gbuffer = bgfx::createFrameBuffer(BX_COUNTOF(gbufferTex), gbufferTex, true);

		m_fxaaBuffer = bgfx::createFrameBuffer(bgfx::BackbufferRatio::Equal, bgfx::TextureFormat::RGBA8, tsFlags | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT);

		// Vertex layout
		PosTexCoord0Vertex::init();

		// Init camera
		cameraCreate();
		cameraSetPosition({0.0f, 1.0f,-2.0f});
		cameraSetVerticalAngle(0.0f);

		// Get renderer capabilities info.
		m_caps = bgfx::getCaps();
		const bgfx::RendererType::Enum renderer = bgfx::getRendererType();
		m_texelHalf = bgfx::RendererType::Direct3D9 == renderer ? 0.5f : 0.0f;

		m_fxaaParams[0] = 0.75f;
		m_fxaaParams[1] = 0.166f;
		m_fxaaParams[2] = 0.0833f;

		m_color[0] = 0.73;
		m_color[1] = 0.875;
		m_color[2] = 0.62;

		imguiCreate();
	}

	int shutdown() override
	{
		meshUnload(m_mesh);

		// Cleanup.
		bgfx::destroy(m_albedoProgram);
		bgfx::destroy(m_fxaaProgram);
		bgfx::destroy(m_blitProgram);

		bgfx::destroy(u_tint);
		bgfx::destroy(u_fxaaParams);
		bgfx::destroy(s_albedo);
		bgfx::destroy(s_source);

		bgfx::destroy(m_gbuffer);
		bgfx::destroy(m_fxaaBuffer);

		cameraDestroy();

		imguiDestroy();

		// Shutdown bgfx.
		bgfx::shutdown();

		return 0;
	}

	bool update() override
	{
		if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState) )
		{
			// Update frame timer
			int64_t now = bx::getHPCounter();
			static int64_t last = now;
			const int64_t frameTime = now - last;
			last = now;
			const double freq = double(bx::getHPFrequency());
			const float deltaTime = float(frameTime/freq);

			// Update camera
			cameraUpdate(deltaTime*0.15f, m_mouseState);

			// Set up matrices for gbuffer
			float view[16];
			cameraGetViewMtx(view);

			float proj[16];
			bx::mtxProj(proj, 60.0f, float(m_width)/float(m_height), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);

			bgfx::setViewRect(RENDER_PASS_ALBEDO, 0, 0, uint16_t(m_width), uint16_t(m_height));
			bgfx::setViewTransform(RENDER_PASS_ALBEDO, view, proj);
			
			bgfx::setViewFrameBuffer(RENDER_PASS_ALBEDO, m_gbuffer);
			bgfx::setState(0
				| BGFX_STATE_WRITE_RGB
				| BGFX_STATE_WRITE_A
				| BGFX_STATE_WRITE_Z);

			float mtx[16] = 
			{ 
				1.f, 0.f, 0.f, 0.f, 
				0.f, 1.f, 0.f, 0.f,
				0.f, 0.f, 1.f, 0.f,
				0.f, 0.f, 0.f, 1.f
			};
			
			bgfx::setUniform(u_tint, &m_color);

			meshSubmit(m_mesh, RENDER_PASS_ALBEDO, m_albedoProgram, mtx);

			// Draw fxaa pass
			bgfx::setTexture(0, s_albedo, bgfx::getTexture(m_gbuffer, 0) );
			bgfx::setUniform(u_fxaaParams, &m_fxaaParams);

			bgfx::setViewFrameBuffer(RENDER_PASS_FXAA, m_fxaaBuffer);

			bgfx::setState(0
				| BGFX_STATE_WRITE_RGB
				| BGFX_STATE_WRITE_A
				);

			// Set up transform matrix for fullscreen quad
			float orthoProj[16];
			bx::mtxOrtho(orthoProj, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f, 0.0f, m_caps->homogeneousDepth);
			bgfx::setViewTransform(RENDER_PASS_FXAA, NULL, orthoProj);
			bgfx::setViewRect(RENDER_PASS_FXAA, 0, 0, uint16_t(m_width), uint16_t(m_height) );
			
			// Bind vertex buffer and draw quad
			screenSpaceQuad( (float)m_width, (float)m_height, m_texelHalf, m_caps->originBottomLeft);
			bgfx::submit(RENDER_PASS_FXAA, m_fxaaProgram);


			bgfx::setTexture(0, s_source, bgfx::getTexture(m_fxaaBuffer, 0) );
			bgfx::setViewFrameBuffer(RENDER_PASS_BLIT, BGFX_INVALID_HANDLE);
			bgfx::setState(0
				| BGFX_STATE_WRITE_RGB
				| BGFX_STATE_WRITE_A
				);

			bgfx::setViewTransform(RENDER_PASS_BLIT, NULL, orthoProj);
			bgfx::setViewRect(RENDER_PASS_BLIT, 0, 0, uint16_t(m_width), uint16_t(m_height) );

			screenSpaceQuad( (float)m_width, (float)m_height, m_texelHalf, m_caps->originBottomLeft);
			bgfx::submit(RENDER_PASS_BLIT, m_blitProgram);

			// Draw UI
			imguiBeginFrame(m_mouseState.m_mx
				,  m_mouseState.m_my
				, (m_mouseState.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
				,  m_mouseState.m_mz
				, uint16_t(m_width)
				, uint16_t(m_height)
				);

			showExampleDialog(this);

			ImGui::SetNextWindowPos(
				  ImVec2(m_width - m_width / 5.0f - 10.0f, 10.0f)
				, ImGuiCond_FirstUseEver
				);
			ImGui::SetNextWindowSize(
				  ImVec2(m_width / 5.0f, m_height / 3.0f)
				, ImGuiCond_FirstUseEver
				);
			ImGui::Begin("Settings"
				, NULL
				, 0
				);

			ImVec2 uv[2];
			uv[1].x = (m_mouseState.m_mx + 8.f) / (float)m_width;
			uv[0].x = (m_mouseState.m_mx - 8.f) / (float)m_width;
			if(m_caps->originBottomLeft) {
				uv[0].y = (m_height-m_mouseState.m_my + 8.f) / (float)m_height;
				uv[1].y = (m_height-m_mouseState.m_my - 8.f) / (float)m_height;
			}
			else {
				uv[0].y = (m_mouseState.m_my + 8.f) / (float)m_height;
				uv[1].y = (m_mouseState.m_my - 8.f) / (float)m_height;
			}

			ImVec2 size( m_width / 5.0f - 16.0f,  m_width / 5.0f - 16.0f);
			
			ImGui::Image(bgfx::getTexture(m_fxaaBuffer, 0) , size, uv[0], uv[1]);

			ImGui::End();

			imguiEndFrame();

			// Advance to next frame. Rendering thread will be kicked to
			// process submitted rendering primitives.
			m_currFrame = bgfx::frame();

			return true;
		}

		return false;
	}

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_debug;
	uint32_t m_reset;

	entry::MouseState m_mouseState;

	// Resource handles
	bgfx::ProgramHandle m_albedoProgram;
	bgfx::ProgramHandle m_fxaaProgram;
	bgfx::ProgramHandle m_blitProgram;

	// Shader uniforms
	bgfx::UniformHandle u_tint;
	bgfx::UniformHandle u_fxaaParams;

	// Uniforms to identify texture samples
	bgfx::UniformHandle s_albedo;
	bgfx::UniformHandle s_source;

	bgfx::FrameBufferHandle m_gbuffer;
	bgfx::FrameBufferHandle m_fxaaBuffer;

	const bgfx::Caps* m_caps;

	Mesh *m_mesh;

	uint32_t m_reading;
	uint32_t m_currFrame;

	float m_color[4];
	float m_fxaaParams[4];

	float m_texelHalf;
};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	  ExampleFXAA
	, "43-fxaa"
	, "FXAA [todo]."
	, "https://bkaradzic.github.io/bgfx/examples.html#fxaa"
	);
