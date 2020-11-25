/*
 * Copyright 2011-2020 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "shaderc.h"

BX_PRAGMA_DIAGNOSTIC_PUSH()
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4100) // error C4100: 'inclusionDepth' : unreferenced formal parameter
BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4265) // error C4265: 'spv::spirvbin_t': class has virtual functions, but destructor is not virtual
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wattributes") // warning: attribute ignored
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wdeprecated-declarations") // warning: ‘MSLVertexAttr’ is deprecated
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wtype-limits") // warning: comparison of unsigned expression in ‘< 0’ is always false
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wshadow") // warning: declaration of 'userData' shadows a member of 'glslang::TShader::Includer::IncludeResult'

#define ENABLE_OPT 1
#include <glslang/Include/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include <SPIRV/GlslangToSpv.h>
#include <SPIRV/GLSL.std.450.h>

#define SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS
#include <spirv_glsl.hpp>

#include <spirv-tools/libspirv.hpp>
#include <spirv-tools/optimizer.hpp>

BX_PRAGMA_DIAGNOSTIC_POP()

#if !defined(GL_FLOAT_VEC4)
#	define GL_FLOAT_VEC4            0x8B52
#endif
#if !defined(GL_FLOAT_MAT3)
#	define GL_FLOAT_MAT3            0x8B5B
#endif
#if !defined(GL_FLOAT_MAT4)
#	define GL_FLOAT_MAT4            0x8B5C
#endif
#if !defined(GL_SAMPLER_2D)
#	define GL_SAMPLER_2D            0x8B5E
#endif
#if !defined(GL_SAMPLER_3D)
#	define GL_SAMPLER_3D            0x8B5F
#endif
#if !defined(GL_SAMPLER_CUBE)
#	define GL_SAMPLER_CUBE          0x8B60
#endif
#if !defined(GL_SAMPLER_2D_SHADOW)
#	define GL_SAMPLER_2D_SHADOW     0x8B62
#endif
#if !defined(GL_SAMPLER_2D_ARRAY)
#	define GL_SAMPLER_2D_ARRAY      0x8DC1
#endif

namespace bgfx { namespace glsl
{
	static const int s_defaultProfile = 100;
	static const EShMessages s_msgCfg = static_cast<EShMessages>(EShMsgRelaxedErrors | EShMsgCascadingErrors | EShMsgDebugInfo);

	static const TBuiltInResource s_resourceLimits =
	{
		32,     // MaxLights
		6,      // MaxClipPlanes
		32,     // MaxTextureUnits
		32,     // MaxTextureCoords
		64,     // MaxVertexAttribs
		4096,   // MaxVertexUniformComponents
		64,     // MaxVaryingFloats
		32,     // MaxVertexTextureImageUnits
		80,     // MaxCombinedTextureImageUnits
		32,     // MaxTextureImageUnits
		4096,   // MaxFragmentUniformComponents
		32,     // MaxDrawBuffers
		128,    // MaxVertexUniformVectors
		8,      // MaxVaryingVectors
		16,     // MaxFragmentUniformVectors
		16,     // MaxVertexOutputVectors
		15,     // MaxFragmentInputVectors
		-8,     // MinProgramTexelOffset
		7,      // MaxProgramTexelOffset
		8,      // MaxClipDistances
		65535,  // MaxComputeWorkGroupCountX
		65535,  // MaxComputeWorkGroupCountY
		65535,  // MaxComputeWorkGroupCountZ
		1024,   // MaxComputeWorkGroupSizeX
		1024,   // MaxComputeWorkGroupSizeY
		64,     // MaxComputeWorkGroupSizeZ
		1024,   // MaxComputeUniformComponents
		16,     // MaxComputeTextureImageUnits
		8,      // MaxComputeImageUniforms
		8,      // MaxComputeAtomicCounters
		1,      // MaxComputeAtomicCounterBuffers
		60,     // MaxVaryingComponents
		64,     // MaxVertexOutputComponents
		64,     // MaxGeometryInputComponents
		128,    // MaxGeometryOutputComponents
		128,    // MaxFragmentInputComponents
		8,      // MaxImageUnits
		8,      // MaxCombinedImageUnitsAndFragmentOutputs
		8,      // MaxCombinedShaderOutputResources
		0,      // MaxImageSamples
		0,      // MaxVertexImageUniforms
		0,      // MaxTessControlImageUniforms
		0,      // MaxTessEvaluationImageUniforms
		0,      // MaxGeometryImageUniforms
		8,      // MaxFragmentImageUniforms
		8,      // MaxCombinedImageUniforms
		16,     // MaxGeometryTextureImageUnits
		256,    // MaxGeometryOutputVertices
		1024,   // MaxGeometryTotalOutputComponents
		1024,   // MaxGeometryUniformComponents
		64,     // MaxGeometryVaryingComponents
		128,    // MaxTessControlInputComponents
		128,    // MaxTessControlOutputComponents
		16,     // MaxTessControlTextureImageUnits
		1024,   // MaxTessControlUniformComponents
		4096,   // MaxTessControlTotalOutputComponents
		128,    // MaxTessEvaluationInputComponents
		128,    // MaxTessEvaluationOutputComponents
		16,     // MaxTessEvaluationTextureImageUnits
		1024,   // MaxTessEvaluationUniformComponents
		120,    // MaxTessPatchComponents
		32,     // MaxPatchVertices
		64,     // MaxTessGenLevel
		16,     // MaxViewports
		0,      // MaxVertexAtomicCounters
		0,      // MaxTessControlAtomicCounters
		0,      // MaxTessEvaluationAtomicCounters = */
		0,      // MaxGeometryAtomicCounters
		8,      // MaxFragmentAtomicCounters
		8,      // MaxCombinedAtomicCounters
		1,      // MaxAtomicCounterBindings
		0,      // MaxVertexAtomicCounterBuffers
		0,      // MaxTessControlAtomicCounterBuffers
		0,      // MaxTessEvaluationAtomicCounterBuffers
		0,      // MaxGeometryAtomicCounterBuffers
		1,      // MaxFragmentAtomicCounterBuffers
		1,      // MaxCombinedAtomicCounterBuffers
		16384,  // MaxAtomicCounterBufferSize
		4,      // MaxTransformFeedbackBuffers
		64,     // MaxTransformFeedbackInterleavedComponents
		8,      // MaxCullDistances
		8,      // MaxCombinedClipAndCullDistances
		4,      // MaxSamples
		256,    // maxMeshOutputVerticesNV
		512,    // maxMeshOutputPrimitivesNV
		32,     // maxMeshWorkGroupSizeX_NV
		1,      // maxMeshWorkGroupSizeY_NV
		1,      // maxMeshWorkGroupSizeZ_NV
		32,     // maxTaskWorkGroupSizeX_NV
		1,      // maxTaskWorkGroupSizeY_NV
		1,      // maxTaskWorkGroupSizeZ_NV
		4,      // maxMeshViewCountNV
		1,      // maxDualSourceDrawBuffersEXT

		{ // limits
			true,   // nonInductiveForLoops
			true,   // whileLoops
			true,   // doWhileLoops
			true,   // generalUniformIndexing
			true,   // generalAttributeMatrixVectorIndexing
			true,   // generalVaryingIndexing
			true,   // generalSamplerIndexing
			true,   // generalVariableIndexing
			true    // generalConstantMatrixVectorIndexing
		}
	};

	static const char* toStr(spv_message_level_t lvl) {
		switch (lvl) {
			case SPV_MSG_FATAL: return "fatal";
			case SPV_MSG_INTERNAL_ERROR: return "internal error";
			case SPV_MSG_ERROR: return "error";
			case SPV_MSG_WARNING: return "warning";
			case SPV_MSG_INFO: return "info";
			case SPV_MSG_DEBUG: return "debug";
			default: return "unknown";
		}
	}

	class Includer : public glslang::TShader::Includer {
	public:
		virtual ~Includer() {}

		IncludeResult* includeSystem(const char* header_name, const char* includer_name, size_t inclusion_depth) override {
			BX_UNUSED(includer_name, inclusion_depth);

			for (auto it : include_path) {
				bx::FilePath path(it.c_str());
				path.join(header_name);

				bx::FileInfo info;
				if (bx::stat(info, path)) {
					if ((info.type == bx::FileType::File) && info.size) {
						bx::Error err;
						IncludeResult* ret = read(header_name, info, err);
						if (ret) {
							return ret;
						}
					}
				}
			}
			return nullptr;
		}

		IncludeResult* includeLocal(const char* header_name, const char* includer_name, size_t inclusion_depth) override {
			BX_UNUSED(inclusion_depth);

			bx::FilePath inc_path(includer_name);
			bx::FilePath path(inc_path.getPath());
			path.join(header_name);
			
			bx::FileInfo info;

			if (!bx::stat(info, path)) {
				bx::printf("[error] failed to retreive file statistics for %s", path.getCPtr());
				return nullptr;
			}
			if ((info.type != bx::FileType::File) || (!info.size)) {
				bx::printf("[error] %s is not a valid file", path.getCPtr());
				return nullptr;
			}
			info.filePath = path;

			bx::Error err;
			IncludeResult *ret = read(header_name, info, err);
			if (!ret) {
				bx::printf("[error] %s", err.getMessage().getPtr());
			}

			return ret;
		}

		void releaseInclude(IncludeResult* result)  override {
			if (result) {
				delete[] result->headerData;
				delete result;
			}
		}

		std::vector<std::string> include_path;

	private:
		IncludeResult* read(const char *header_name, bx::FileInfo const& info, bx::Error &err) {
			bx::FileReader input;
			char* data = new char[info.size];
			
			if (input.open(info.filePath, &err)) {
				int32_t nread = input.read(data, static_cast<int32_t>(info.size), &err);
				if (err.isOk()) {
					return new IncludeResult(header_name, data, nread, data);
				}
				input.close();
			}
			delete[] data;
			return nullptr;
		}
	};

	static glslang::TShader* parse(EShLanguage stage, std::string const& code, std::string const& filename, Includer &includer) {
		const char* data = code.c_str();
		int len = static_cast<int>(code.size());

		const char* filepath = filename.c_str();

		glslang::TShader *shader = new glslang::TShader(stage);
		shader->setStringsWithLengthsAndNames(&data, &len, &filepath, 1);
		shader->setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientOpenGL, s_defaultProfile);
		shader->setEnvClient(glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
		shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
		shader->setAutoMapLocations(true);
		//shader->setAutoMapBindings(false);
		// [todo] use GL_ARB_shading_language_include when it's supported
		shader->setPreamble("#extension GL_GOOGLE_include_directive : require\n");
		bool ret = shader->parse(&s_resourceLimits, s_defaultProfile, false, s_msgCfg, includer);
		if (!ret) {
			bx::printf("[error] failed to parse %s\n%s\n", filepath, shader->getInfoLog());
			delete shader;
			return nullptr;
		}
		return shader;
	}

	static bool toSpirv(glslang::TProgram &program, EShLanguage stage, bool optimize, std::vector<unsigned int> &bytecode) {
		bytecode.clear();

		const glslang::TIntermediate* im = program.getIntermediate((EShLanguage)stage);
		if (!im) {
			return false;
		}

		spv::SpvBuildLogger logger;
		glslang::SpvOptions opts;
		opts.generateDebugInfo = false;
		opts.stripDebugInfo = false;
		opts.disableOptimizer = true;
		opts.optimizeSize = false;
		opts.disassemble = false;
		opts.validate = true;
		
		glslang::GlslangToSpv(*im, bytecode, &logger, &opts);
		if (bytecode.empty()) {
			std::string spv_log = logger.getAllMessages();
			if (!spv_log.empty()) {
				bx::printf("%s\n", spv_log.c_str());
			}
			return false;
		}

		spvtools::Optimizer optimizer(SPV_ENV_UNIVERSAL_1_5);
		optimizer.SetMessageConsumer(
			[](spv_message_level_t lvl, const char* source, const spv_position_t& position, const char* msg) {
				bx::printf("[%s] %s+%d: %s\n", toStr(lvl), source, position.line, msg);
			}
		);

		optimizer.RegisterLegalizationPasses();
		if(optimize)
		{
			optimizer.RegisterPerformancePasses();
		}
		
		spvtools::OptimizerOptions optimizer_opts;
		optimizer_opts.set_run_validator(true);
		optimizer_opts.set_preserve_bindings(false);
		optimizer.Run(bytecode.data(), bytecode.size(), &bytecode, optimizer_opts);

		return true;
	}

	static bool toGLSL(std::vector<unsigned int> &bytecode, uint32_t version, bool es, std::string &out) {
		spirv_cross::CompilerGLSL *compiler = new spirv_cross::CompilerGLSL(bytecode.data(), bytecode.size());
		
		spirv_cross::CompilerGLSL::Options opts;
		opts.version = version;
		opts.es = es;
		compiler->set_common_options(opts);

		bool ret = true;
		out.clear();
		
		out = compiler->compile();				// exceptions are disabled. The program will brutally exit if an error occured.
		if(out.empty()) {
			bx::printf("Compilation error!\n");
			ret = false;
		}
		
		delete compiler;
		return ret;
	}

	static bool compile(const Options& _options, uint32_t _version, const std::string& _code, bx::WriterI* _writer)
	{
		EShLanguage stage;
		switch(_options.shaderType)
		{
			case 'f':
				stage = EShLangFragment;
				break;
			case 'c':
				stage = EShLangCompute;
				break;
			case 'v':
				stage = EShLangVertex;
				break;
			default:
				bx::printf("Unsupported shader type: %c\n", _options.shaderType);
				return false;
		}

		bool es = _version & 0x80000000;
		uint32_t profile = _version & 0x7fffffff;

		bgfx::glsl::Includer includer;
		includer.include_path = _options.includeDirs;

		glslang::TShader *shader = bgfx::glsl::parse(stage, _code, _options.inputFilePath, includer);
		if(!shader)
		{
			return false;
		}

		bool ret;
		glslang::TProgram *program = new glslang::TProgram;
		
		program->addShader(shader);
		ret = program->link(bgfx::glsl::s_msgCfg);
		if(ret) {
			UniformArray uniforms;
			if(program->buildReflection()) {
				// Retrieve uniforms and try to reorganise their locations.
				int location = 0;
				int count = program->getNumUniformVariables();
				uniforms.resize(count);

				for(int i=0; i<count; i++) {
					const glslang::TObjectReflection &refl = program->getUniform(i);

					shader->addUniformLocationOverride(refl.name.c_str(), location);		// [todo] maybe this should be done in a custom TIoMapResolver/TIoMapper
					int num = 1;
					if(refl.getType()->isArray()) {
						num = refl.getType()->getArraySizes()->getCumulativeSize();
					}
					
					uniforms[i].name = refl.name;
					uniforms[i].num = num;
					uniforms[i].regIndex = location;
					uniforms[i].regCount = num;
					
					location += num;

					switch(refl.glDefineType) {
						case GL_SAMPLER_2D:
						case GL_SAMPLER_3D:
						case GL_SAMPLER_CUBE:
						case GL_SAMPLER_2D_SHADOW:
						case GL_SAMPLER_2D_ARRAY:
							uniforms[i].type = UniformType::Sampler;
							break;
						case GL_FLOAT_VEC4:
							uniforms[i].type = UniformType::Vec4;
							break;
						case GL_FLOAT_MAT3:
							uniforms[i].type = UniformType::Mat3;
							break;
						case GL_FLOAT_MAT4:
							uniforms[i].type = UniformType::Mat4;
							break;
						default:
							uniforms[i].type = UniformType::End;
							break;
					}
				}
			}

			ret = program->mapIO();
			if(ret)
			{
				std::string out;
				std::vector<unsigned int> bytecode;
				if (bgfx::glsl::toSpirv(*program, stage, _options.optimize, bytecode)) {
					ret = bgfx::glsl::toGLSL(bytecode, profile, es, out);
				}

				if(ret) 
				{
					uint32_t shader_size = static_cast<uint32_t>(out.size());
					uint16_t count = (uint16_t)uniforms.size();
					bx::write(_writer, count);

					for (UniformArray::const_iterator it = uniforms.begin(); it != uniforms.end(); ++it)
					{
						const Uniform& un = *it;
						uint8_t nameSize = (uint8_t)un.name.size();
						bx::write(_writer, nameSize);
						bx::write(_writer, un.name.c_str(), nameSize);
						uint8_t uniformType = uint8_t(un.type);
						bx::write(_writer, uniformType);
						bx::write(_writer, un.num);
						bx::write(_writer, un.regIndex);
						bx::write(_writer, un.regCount);
						bx::write(_writer, un.texComponent);
						bx::write(_writer, un.texDimension);

						BX_TRACE("%s, %s, %d, %d, %d"
							, un.name.c_str()
							, getUniformTypeName(un.type)
							, un.num
							, un.regIndex
							, un.regCount
							);
					}

					bx::write(_writer, shader_size);
					bx::write(_writer, out.c_str(), shader_size);

					uint8_t nul = 0;
					bx::write(_writer, nul);
				}
			}
			else
			{
				bx::printf("Failed to map IO: %s\n", program->getInfoLog());		
			}
		}
		else
		{
			bx::printf("Failed to link program: %s\n", program->getInfoLog());
		}

		delete program;
		delete shader;
		return ret;
	}

} // namespace glsl

	bool compileGLSLShader(const Options& _options, uint32_t _version, const std::string& _code, bx::WriterI* _writer)
	{
		glslang::InitializeProcess();
		bool ret = glsl::compile(_options, _version, _code, _writer);
		glslang::FinalizeProcess();
		return ret;
	}

} // namespace bgfx
