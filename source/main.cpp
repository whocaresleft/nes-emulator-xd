#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/backends/imgui_impl_opengl3.h"
#include "../third_party/imgui/backends/imgui_impl_sdl2.h"
#include <SDL.h>
#include <SDL_opengl.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <iostream>
#include <cstdio>
#include "../header/utility.h"
#include "../header/cpu.h"

typedef struct _gui_context {
	struct _cpu {
		bool has_started;
		float last_freq;
		bool hide;
		bool register_view;
		bool instruction_view;
		struct registers {
			u8 a, x, y, sp, p;
			u16 pc;
		} reg;
		struct _opcode {
			bool fetched;
			bool executed;
			const instruction* instr;

			int operands[2];
		} opcode;
	} cpu;
	bool show_rom;
	bool show_vram;
} _gui_context;

struct _gui_context;
void cpu_window(_gui_context::_cpu*, cpu*);

void gui_fetch(_gui_context::_cpu*, cpu*);
void gui_decode(_gui_context::_cpu*, cpu*);
void gui_execute(_gui_context::_cpu*, cpu*);

int main(int argc, char* argv[]) {

	_gui_context ctx = { 
		{false, 0.0f,  true, false, false,{0,0,0,0,0,0}, { false, false, nullptr, { 0x00, 0x00 } }},
		false, 
		false,
	};

	// SDL Context
#ifdef _WIN32
	::SetProcessDPIAware();
#endif
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
		printf("Error during SDL Context initialization: %s", SDL_GetError());
		return 1;
	}

	const char* glsl_version = "#version 130";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

#ifdef SDL_HINT_IME_SHOW_UI
	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	float main_scale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_Window* window = SDL_CreateWindow(
		"AAA", 
		SDL_WINDOWPOS_CENTERED, 
		SDL_WINDOWPOS_CENTERED,
		static_cast<int>(900 * main_scale),
		static_cast<int>(500 * main_scale),
		window_flags
	);

	if (window == nullptr) {
		printf("Error during SDL Window creation: %s", SDL_GetError());
		return 1;
	}

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	if (gl_context == nullptr) {
		printf("Error during SDL GL Context creation: %s", SDL_GetError());
	}

	SDL_GL_MakeCurrent(window, gl_context);
	SDL_GL_SetSwapInterval(1);

	// ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);
	style.ScaleAllSizes(main_scale);
	style.FontScaleDpi = main_scale;

	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init(glsl_version);

	bool show_demo_window = true;
	bool done = false;
	std::vector<u8> x = read_file(".\\resource\\nestest.nes");
	cartridge c(x);

	cpu* CPU = new cpu();
	CPU->load(&c);

	while (!done) {

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_KEYDOWN) {
				switch (event.key.keysym.sym) {
				case SDLK_ESCAPE:
					done = true;
					break;

				case SDLK_r:
					show_demo_window = false;
					break;
				case SDLK_l:
					show_demo_window = true;
					break;
				}
			}
			if ((event.type == SDL_QUIT)
				|| (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))) done = true;
		}
		if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) { SDL_Delay(10); continue; }

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		/* * * * * * * * * * * Main Window * * * * * * * * * * */

		if (show_demo_window) {
			ImGui::ShowDemoWindow(& show_demo_window);
		}
		else {
			if (ImGui::BeginMainMenuBar()) {

				if (ImGui::BeginMenu("CPU")) {
					if (ImGui::MenuItem(ctx.cpu.hide ? "Show" : "Hide")) {
						ctx.cpu.hide = !ctx.cpu.hide;
						ctx.cpu.register_view &= !(ctx.cpu.hide);
						ctx.cpu.instruction_view &= !(ctx.cpu.hide);
					}
					ImGui::Separator();
					ImGui::MenuItem("Registers view", nullptr, &ctx.cpu.register_view, !ctx.cpu.hide);
					ImGui::MenuItem("Instruction view", nullptr, &ctx.cpu.instruction_view, !ctx.cpu.hide);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("VRAM")) {
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("ROM")) {
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("PPU")) {
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Screen")) {
					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();
			}

			if (!ctx.cpu.hide) {
				cpu_window(&(ctx.cpu), CPU);
			}
		}

		/* * * * * * * * * * Main Window End * * * * * * * * * */

		ImGui::Render();
		glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
	}

	// Cleanup
	delete CPU;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}


void cpu_window(_gui_context::_cpu* ctx, cpu* CPU) {

	if (!ImGui::Begin("CPU", nullptr, ImGuiWindowFlags_NoResize)) {
		ImGui::End();
		return;
	}

	ImGui::SeparatorText("CPU Status");
	if (CPU->halted.load()) {
		ImGui::Text("Halted");
		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
		ImGui::PlotLines("CPU Frequency", {0}, 1, 0, nullptr, -1.f, 1.f, ImVec2(100.f, 20.f));
		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
		if (ImGui::Button("Reset")) {
			CPU->reset();
			ctx->has_started = false;
		}
	}
	else {
		if (CPU->paused.load()) {
			ImGui::Text("Paused");
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
			float freq[1] = { ctx->last_freq };
			ImGui::PlotLines("CPU Frequency", freq, 1, 0, nullptr, -1.f, 1.f, ImVec2(100.f, 20.f));
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();

			if (ctx->has_started) {
				if (ImGui::Button("Resume")) {
					CPU->resume();
				}
			}
			else {
				if (ImGui::Button("Start")) {
					CPU->run_async(
						[](cpu&) {},
						[ctx](cpu& cpu) {
							ctx->reg.a = cpu.a;
							ctx->reg.x = cpu.x;
							ctx->reg.y = cpu.y;
							ctx->reg.p = cpu.p;
							ctx->reg.sp = cpu.sp;
							ctx->reg.pc = cpu.pc;
							ctx->opcode.instr = &INSTRUCTIONS[cpu.opcode];
							{
								usize len = ADDRESSING_MODES[ctx->opcode.instr->mode].length;
								if (len > 1) ctx->opcode.operands[0] = cpu.read_u8(cpu.pc + 1);
								if (len > 2) ctx->opcode.operands[1] = cpu.read_u8(cpu.pc + 2);
							}
						}
					);
					ctx->has_started = true;
				}
			}

		}
		else {
			ImGui::Text("Running");
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
			float freq[10];
			for (uint8_t n = 0; n < 10; n++) freq[n] = sinf(n * 5.f + static_cast<float>(ImGui::GetTime()) * 5.f);
			ImGui::PlotLines("CPU Frequency", freq, 10, 0, nullptr, -1.f, 1.f, ImVec2(100.f, 20.f));
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
			if (ImGui::Button("Pause")) {
				CPU->pause();
				ctx->last_freq = freq[9];
			}
		}
		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
		if (ImGui::Button("Stop")) {
			CPU->stp(ADDRESSING_MODES[addressing_mode_type::implied]);
		}
	}

	if (ctx->register_view) {
		// Start - Register View
		ImGui::SeparatorText("Registers");

		// Registers: A, X, Y, PC, SP and P
		ImGui::BeginChild("register-panel", ImVec2(150.0f, 65.f));
		{
			{
				ImGui::BeginGroup();
				ImGui::Text("A:    0x%02X", ctx->reg.a);
				ImGui::Text("X:    0x%02X", ctx->reg.x);
				ImGui::Text("Y:    0x%02X", ctx->reg.y);
				ImGui::EndGroup();
			}
			ImGui::SameLine();
			{
				ImGui::BeginGroup();
				ImGui::Text("PC: 0x%04X", ctx->reg.pc);
				ImGui::Text("SP:   0x%02X", ctx->reg.sp);
				ImGui::Text("P:    0x%02X", ctx->reg.p);
				ImGui::EndGroup();
			}
		}
		ImGui::EndChild();

		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();

		// Status flags - Shows each flag in the P register
		ImGui::BeginChild("status", ImVec2(0.f, 65.f));
		{
			ImGui::SeparatorText("Status (P) flags");
			u8 p = ctx->reg.p;
			ImGui::BeginGroup();
			{
				ImGui::BeginDisabled(!(p & 0b10000000)); ImGui::Text("Negative"); ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::BeginDisabled(!(p & 0b1000000)); ImGui::Text("Overflow"); ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::BeginDisabled(!(p & 0b100000)); ImGui::Text("Unused "); ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::BeginDisabled(!(p & 0b10000)); ImGui::Text("Break "); ImGui::EndDisabled();
			}
			ImGui::EndGroup();
			ImGui::BeginGroup();
			{
				ImGui::BeginDisabled(!(p & 0b1000)); ImGui::Text("Decimal "); ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::BeginDisabled(!(p & 0b100)); ImGui::Text("Int-Dis "); ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::BeginDisabled(!(p & 0b10)); ImGui::Text("Zero   "); ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::BeginDisabled(!(p & 0b1)); ImGui::Text("Carry "); ImGui::EndDisabled();
			}
			ImGui::EndGroup();
		}
		ImGui::EndChild();

		if (ImGui::TreeNode("Stack Content")) {
			ImGui::Text("");
			ImGui::TreePop();
		}

		// End - Register View
	}
	if (ctx->instruction_view) {
		// Start - Instruction View
		ImGui::SeparatorText("Current Instruction");
		ImGui::BeginChild("instruction-panel");

		// pc, buttons (fetch, decode, execute)
		ImGui::Text("PC: 0x%04X", ctx->reg.pc);
		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();

		ImGui::BeginDisabled(CPU->halted.load() || !CPU->paused.load());
		ImGui::BeginDisabled(ctx->opcode.fetched);
		if (ImGui::Button("Fetch")) {
			gui_fetch(ctx, CPU);
		} 
		ImGui::EndDisabled();

		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();

		ImGui::BeginDisabled(!ctx->opcode.fetched);
		if (ImGui::Button("Decode")) {
			gui_decode(ctx, CPU);
		}
		ImGui::EndDisabled();

		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();

		ImGui::BeginDisabled(ctx->opcode.executed || !ctx->opcode.fetched);
		if (ImGui::Button("Execute")) {
			gui_execute(ctx, CPU);
		}
		ImGui::EndDisabled();

		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();

		if (ImGui::Button("Step")) {
			if (!ctx->opcode.fetched) gui_fetch(ctx, CPU);
			gui_decode(ctx, CPU);
			gui_execute(ctx, CPU);
		}
		ImGui::EndDisabled();

		ImGui::Spacing();

		const instruction* _instr = ctx->opcode.instr;
		if (_instr != nullptr) {
			const addressing_mode mode = ADDRESSING_MODES[_instr->mode];

			ImGui::Text("OpCode: 0x%02X", _instr->opcode);
			ImGui::SameLine();
			if (mode.length > 1) ImGui::Text("0x%02X", ctx->opcode.operands[0]);
			else ImGui::Text("    ");
			ImGui::SameLine();
			if (mode.length > 2) ImGui::Text("0x%02X", ctx->opcode.operands[1]);
			else ImGui::Text("    ");
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
			ImGui::Text("%s", _instr->mnemonic);

			ImGui::Spacing();
			ImGui::Text("Addressing Mode: &s", ADDRESSING_MODE_NAMES[_instr->mode]);
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
			ImGui::Text("Bytes:  %d", mode.length);
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
			ImGui::Text("Cycles: %d", _instr->cycles);
		}

		ImGui::EndChild();
		// End - Instruction View
	}
	ImGui::End();
}

void gui_fetch(_gui_context::_cpu* ctx, cpu* CPU) {
	CPU->fetch();
	ctx->opcode.fetched = true;
	ctx->opcode.executed = false;
}
void gui_decode(_gui_context::_cpu* ctx, cpu* CPU) {
	CPU->decode();
	ctx->opcode.instr = CPU->decoded;
	usize len = ADDRESSING_MODES[CPU->decoded->mode].length;
	if (len > 1) { ctx->opcode.operands[0] = CPU->read_u8(CPU->pc); }
	if (len > 2) { ctx->opcode.operands[1] = CPU->read_u8(CPU->pc + 1); }
}
void gui_execute(_gui_context::_cpu* ctx, cpu* CPU) {
	CPU->execute();
	ctx->opcode.fetched = false;
	ctx->opcode.executed = true;
}