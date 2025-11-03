#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/backends/imgui_impl_opengl3.h"
#include "../third_party/imguifiledialog/ImGuiFileDialog.h"
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
#include <cmath>
#include <span>
struct _ram {
	bool show;
	u32 start;
	u32 end;
	bool mirrored;
	u32 look_for;
	std::span<u8> view;
	u16 min;
	u16 max;

	_ram(u16 min, u16 max) :
		show(false),
		start(0x0000_u16),
		end(0x002F_u16),
		mirrored(false),
		look_for(0x0000_u16),

		min(min),
		max(max),
		view()
	{
	}
};


typedef struct _gui_context {
	struct _rom {
		bool hide;
		bool inserted, loaded;
		bool show_prg, show_chr;
		std::filesystem::path game_path_opened, game_path_loaded;
		cartridge* game_opened;
		cartridge* game_loaded;

		_rom() :
			hide(true),
			inserted(false),
			loaded(false),
			game_opened(nullptr),
			game_loaded(nullptr),
			game_path_opened(),
			game_path_loaded(),
			show_chr(false),
			show_prg(false)
		{}
	} rom;

	struct _cpu {

		bool something_loaded;
		bool has_started;
		float last_freq;
		bool hide;
		bool register_view;
		bool instruction_view;
		bool stack_view;
		usize cycles;
		struct _ram wram;
		struct _registers {
			u8 a, x, y, sp, p;
			u16 pc;
			_registers() : 
				a(0x00),
				x(0x00),
				y(0x00),
				sp(0xFF),
				p(0x00),
				pc(0x0000)
			{}
		} registers;
		struct _opcode {
			bool fetched;
			bool executed;
			const instruction* instr;

			int operands[2];

			_opcode() :
				fetched(false),
				executed(false),
				instr(nullptr),
				operands(0, 0)
			{}
		} opcode;
	
		_cpu() :
			something_loaded(false),
			has_started(false),
			last_freq(.0f),
			hide(true),
			register_view(false),
			instruction_view(false),
			stack_view(false),
			cycles(7_usize),
			wram(0x0000_u16, 0x07FF_u16),
			registers(),
			opcode()
		{}
	} cpu;

	struct _ppu {
	
		bool hide;
		bool register_view;
		bool oam_memory_show;
		bool palette_show;
		u8 ctrl;
		u8 mask;
		u8 status;
		u8 oamaddr;
		u8 internal_buffer;
		u16 v, t;
		u8 w_x;
		usize cycles;

		std::span<u8> color_palette;
		std::span<u8> oam_memory;

		struct _ram vram;

		_ppu() :
			hide(true),
			ctrl(0),
			mask(0),
			status(0),
			oamaddr(0),
			internal_buffer(0),
			palette_show(false),
			oam_memory_show(false),
			v(0),
			t(0),
			w_x(0),
			cycles(0),
			vram(0x0000_u16, 0x07FF_u16),
			register_view(false)
		{}
	} ppu;

	_gui_context() : 
		rom(),
		cpu(),
		ppu()
	{}
} _gui_context;

void ppu_window(_gui_context::_ppu*, ppu*);
void rom_window(_gui_context::_rom*, cpu*);
void cpu_window(_gui_context::_cpu*, cpu*);

inline void static gui_fetch(_gui_context::_cpu*, cpu*);
inline void static gui_decode(_gui_context::_cpu*, cpu*);
inline void static gui_execute(_gui_context::_cpu*, cpu*);

inline void static update_ppu_context(_gui_context::_ppu* ctx, ppu* ppu) {
	//ctx->cycles = ppu->get_cycles();

	ctx->ctrl = 0;
	ctx->mask = 0;
	ctx->status = 0;
	ctx->oamaddr = 0;
	ctx->internal_buffer = 0;
	ctx->v = 0;
	ctx->t = 0;
	ctx->w_x = 0;
}

inline void static update_cpu_registers(_gui_context::_cpu* ctx, cpu* cpu) {
	ctx->registers.pc = cpu->pc;
	ctx->cycles = cpu->cycles;
	if (ctx->register_view) {
		ctx->registers.a = cpu->a;
		ctx->registers.x = cpu->x;
		ctx->registers.y = cpu->y;
		ctx->registers.p = cpu->p;
		ctx->registers.sp = cpu->sp;
	}
}
inline void static update_cpu_instruction(_gui_context::_cpu* ctx, cpu* cpu) {
	if (ctx->instruction_view) {
		ctx->opcode.instr = &INSTRUCTIONS[cpu->opcode];
		usize len = ADDRESSING_MODES[ctx->opcode.instr->mode].length;
		if (len > 1) ctx->opcode.operands[0] = cpu->read_u8(cpu->pc + 1);
		if (len > 2) ctx->opcode.operands[1] = cpu->read_u8(cpu->pc + 2);
	}
}

bool InputHex(const char* label, u32* value, int step = 1, int step_fast = 16, int digits = 2, ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsHexadecimal) {
	char* fmt = new char[digits + 1];
	std::snprintf(fmt, digits + 1, "%%0%dX", digits);
	bool to_ret = ImGui::InputScalar(label, ImGuiDataType_U32, value, &step, &step_fast, fmt, flags);
	delete [] fmt;
	return to_ret;
}

int main(int argc, char* argv[]) {

	_gui_context* ctx = new _gui_context();

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

	cpu* CPU = new cpu();
	ctx->cpu.wram.view = CPU->get_wram();
	update_cpu_registers(&ctx->cpu, CPU);

	ppu* PPU = CPU->get_ppu();
	ctx->ppu.color_palette = PPU->get_color_palette();
	ctx->ppu.vram.view = PPU->get_vram();
	ctx->ppu.oam_memory = PPU->get_oam_memory();

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
					if (ImGui::MenuItem(ctx->cpu.hide ? "Show" : "Hide")) {
						ctx->cpu.hide = !ctx->cpu.hide;
						ctx->cpu.register_view &= !(ctx->cpu.hide);
						ctx->cpu.instruction_view &= !(ctx->cpu.hide);
						ctx->cpu.wram.show &= !(ctx->cpu.hide);
					}
					ImGui::Separator();
					ImGui::MenuItem("Registers view", nullptr, &ctx->cpu.register_view, !ctx->cpu.hide);
					ImGui::MenuItem("Instruction view", nullptr, &ctx->cpu.instruction_view, !ctx->cpu.hide);
					ImGui::Separator();
					ImGui::MenuItem("RAM view", nullptr, &ctx->cpu.wram.show, !ctx->cpu.hide);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("ROM")) {
					if (ImGui::MenuItem(ctx->rom.hide ? "Show" : "Hide")) {
						ctx->rom.hide = !ctx->rom.hide;
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("PPU")) {
					if (ImGui::MenuItem(ctx->ppu.hide ? "Show" : "Hide")) {
						ctx->ppu.hide = !ctx->ppu.hide;
						ctx->ppu.register_view &= !(ctx->ppu.hide);
						ctx->ppu.vram.show &= !(ctx->ppu.hide);
					}
					ImGui::Separator();
					ImGui::MenuItem("Registers view", nullptr, &ctx->ppu.register_view, !ctx->ppu.hide);
					ImGui::Separator();
					ImGui::MenuItem("VRAM view", nullptr, &ctx->ppu.vram.show, !ctx->ppu.hide);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Screen")) {
					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();
			}

			if (!ctx->cpu.hide) { cpu_window(&(ctx->cpu), CPU); }
			if (!ctx->rom.hide) { rom_window(&(ctx->rom), CPU); }
			if (!ctx->ppu.hide) { ppu_window(&(ctx->ppu), PPU); }
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
	if (ctx->rom.game_opened != nullptr) { delete ctx->rom.game_opened; }
	if (ctx->rom.game_loaded != nullptr) { delete ctx->rom.game_loaded; }
	delete ctx;
	delete CPU;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

void ppu_window(_gui_context::_ppu* ctx, ppu* PPU) {

	ImGui::SetNextWindowSizeConstraints(ImVec2(440.f, 90.f), ImVec2(920.f, 700.f));
	if (!ImGui::Begin("PPU", nullptr, ImGuiWindowFlags_HorizontalScrollbar)) {
		ImGui::End(); 
		return;
	}

	ImGui::SeparatorText("PPU Status");
	float freq[20];
	for (uint8_t n = 0; n < 20; n++) freq[n] = sinf(n * 5.f + static_cast<float>(ImGui::GetTime()) * 15.f);
	ImGui::PlotLines("PPU Frequency", freq, 20, 0, nullptr, -1.f, 1.f, ImVec2(100.f, 20.f));

	ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
	ImGui::Text("Cycles: %d    Scanline: %d", ctx->cycles, 0);

	if (ctx->register_view) {
		ImGui::SeparatorText("Register view");
		ImGui::BeginChild("register-view", ImVec2(420.f + (ctx->oam_memory_show ? 450.f : 0.0f), 150.f + (ctx->oam_memory_show ? 150.f : 0.f) + (ctx->palette_show ? 200.f : 0.f)));
	
		ImGui::BeginGroup(); {
			ImGui::Text("Ctrl:   "); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->ctrl & 0b10000000)); ImGui::Text("V"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->ctrl & 0b01000000)); ImGui::Text("P"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->ctrl & 0b00100000)); ImGui::Text("H"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->ctrl & 0b00010000)); ImGui::Text("B"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->ctrl & 0b00001000)); ImGui::Text("S"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->ctrl & 0b00000100)); ImGui::Text("I"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->ctrl & 0b00000010)); ImGui::Text("N"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->ctrl & 0b00000001)); ImGui::Text("N"); ImGui::EndDisabled();

			ImGui::Text("Mask:   "); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->mask & 0b10000000)); ImGui::Text("B"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->mask & 0b01000000)); ImGui::Text("G"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->mask & 0b00100000)); ImGui::Text("R"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->mask & 0b00010000)); ImGui::Text("s"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->mask & 0b00001000)); ImGui::Text("b"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->mask & 0b00000100)); ImGui::Text("M"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->mask & 0b00000010)); ImGui::Text("m"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->mask & 0b00000001)); ImGui::Text("G"); ImGui::EndDisabled();

			ImGui::Text("Status: "); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->status & 0b10000000)); ImGui::Text("V"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->status & 0b01000000)); ImGui::Text("S"); ImGui::EndDisabled(); ImGui::SameLine();
			ImGui::BeginDisabled(!(ctx->status & 0b00100000)); ImGui::Text("O"); ImGui::EndDisabled();

			ImGui::Text("OAM Address: 0x%02X", ctx->oamaddr);
		} ImGui::EndGroup();

		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
		
		ImGui::BeginGroup(); {

			ImGui::Text("v: 0x%04X", ctx->v & 0x7FFF);
			ImGui::Text("t: 0x%04X", ctx->t & 0x7FFF);
			ImGui::Text("x: 0x%01X", ctx->w_x & 0x07);
			ImGui::BeginDisabled(!(ctx->w_x & 0xF0)); ImGui::Text("w"); ImGui::EndDisabled();

		} ImGui::EndGroup();

		ImGui::Text("Internal data buffer: 0x%02X", ctx->internal_buffer);

		ctx->palette_show = ImGui::TreeNode("Palette Table");
		if (ctx->palette_show) {
			ImGui::SeparatorText("Background palette");
			ImGui::BeginChild("background-palette", ImVec2(380.f, 85.f), ImGuiChildFlags_Borders);
			for (usize i = 0; i < 8; ++i) {
				ImGui::BeginGroup();
				ImGui::Text("$%04X", (0x3F00 + i));
				ImGui::Text(" 0x%02X", ctx->color_palette[i]);
				ImGui::Spacing();
				ImGui::Text("$%04X", (0x3F00 + i + 8));
				ImGui::Text(" 0x%02X", ctx->color_palette[i + 8]);
				ImGui::EndGroup();
				ImGui::SameLine();
			}
			ImGui::EndChild();

			ImGui::SeparatorText("Sprite palette");
			ImGui::BeginChild("sprite-palette", ImVec2(380.f, 85.f), ImGuiChildFlags_Borders);
			for (usize i = 0; i < 8; ++i) {
				ImGui::BeginGroup();
				ImGui::Text("$%04X", (0x3F10 + i));
				ImGui::Text(" 0x%02X", ctx->color_palette[i]);
				ImGui::Spacing();
				ImGui::Text("$%04X", (0x3F10 + i + 8));
				ImGui::Text(" 0x%02X", ctx->color_palette[i + 8]);
				ImGui::EndGroup();
				ImGui::SameLine();
			}
			ImGui::EndChild();
			ImGui::TreePop();
		}
		ImGui::Checkbox("OAM Data", &ctx->oam_memory_show);
		ImGui::EndChild();
	}


	if (ctx->oam_memory_show) {
		if (!ImGui::Begin("OAM DATA")) {
			ImGui::End();
			ImGui::End();
			return;
		}
		float row_height = ImGui::GetTextLineHeightWithSpacing();
		ImVec2 outer_size = ImVec2(0.0f, row_height * 4);
		if (
			ImGui::BeginTable
				(
					"oam-data-table",
					17,
					ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
					outer_size
				)
		) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("v + >");
			for (int i = 0x0; i <= 0xF; ++i) {
				char label[5];
				std::snprintf(label, sizeof(label), "0x%02X", i);
				ImGui::TableSetupColumn(label);
			}
			ImGui::TableHeadersRow();
			int column = 0;
			char row_label[7];
			char cell_label[5];
			// Intermediate rows
			for (int row = 0; row < 16; ++row) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				std::snprintf(row_label, sizeof(row_label), "0x%02X", (row << 4));
				ImGui::TextUnformatted(row_label);
				for (int column = 0; column < 16; ++column) {
					ImGui::TableSetColumnIndex(column + 1);
					std::snprintf(cell_label, sizeof(cell_label), "0x%02X", ctx->oam_memory[(row << 4) + column]);
					ImGui::TextUnformatted(cell_label);
				}
			}

			ImGui::EndTable();
		}
		ImGui::End();
	}

	if (ctx->vram.show) {
		if (!ImGui::Begin("PPU Vram")) {
			ImGui::End();
			ImGui::End();
			return;
		}

		ImGui::SeparatorText("Address Range");
		ImGui::SetNextItemWidth(150.f);
		InputHex("Start", &ctx->vram.start, 1, 16, 4);
		ctx->vram.start = static_cast<u32>(std::clamp(static_cast<u16>(ctx->vram.start & 0x0000FFFF), 0_u16, ctx->vram.max));

		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::SetNextItemWidth(100.f);
		InputHex("Addr##", &ctx->vram.look_for, 1, 16, 4);
		ImGui::SameLine();
		char go_buf[13];
		std::snprintf(go_buf, sizeof(go_buf), "Go to 0x%04X", ctx->vram.look_for);
		bool go_to_button_pressed = ImGui::Button(go_buf);


		ImGui::SetNextItemWidth(150.f);
		InputHex("End", &ctx->vram.end, 1, 16, 4);
		ctx->vram.end = static_cast<u32>(std::clamp(static_cast<u16>(ctx->vram.end & 0x0000FFFF), static_cast<u16>(ctx->vram.start & 0x0000FFFF), ctx->vram.max));

		ImGui::SeparatorText("Memory View");

		int start_aligned = ctx->vram.start & ~0x000F;
		int end_aligned = ctx->vram.end | 0x000F;
		int element_count = 1 + end_aligned - start_aligned;
		int row_count = element_count / 16;

		int height_multiplier = row_count > 2 ? 4 : (row_count > 1 ? 3 : 2);
		float row_height = ImGui::GetTextLineHeightWithSpacing();
		ImVec2 outer_size = ImVec2(0.0f, row_height * height_multiplier);
		if (
			ImGui::BeginTable(
				"wram-table",
				17,
				ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
				outer_size
			)
			) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("v + >");
			for (int i = 0x0; i <= 0xF; ++i) {
				char label[5];
				std::snprintf(label, sizeof(label), "0x%02X", i);
				ImGui::TableSetupColumn(label);
			}
			ImGui::TableHeadersRow();

			/* If off_start, start with ---- on the first row
			 * If off_end, end with ---- on the last row
			 *
			 * But, if start_aligned == end_aligned & ~0x000F, same row
			 */
			int off_start = ctx->vram.start - start_aligned;
			int off_end = end_aligned - ctx->vram.end;
			if (start_aligned == (end_aligned & ~0x000F)) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				char row_label[7];
				std::snprintf(row_label, sizeof(row_label), "0x%04X", start_aligned);
				ImGui::TextUnformatted(row_label);
				int column = 0;

				for (column; column < off_start; ++column) {
					ImGui::TableSetColumnIndex(column + 1);
					ImGui::TextUnformatted("----");
				}
				char cell_label[5];
				for (column; column < (0xF - off_end) + 1; ++column) {
					ImGui::TableSetColumnIndex(column + 1);
					std::snprintf(cell_label, sizeof(cell_label), "0x%02X", ctx->vram.view[start_aligned + column]);
					ImGui::TextUnformatted(cell_label);
				}
				for (column; column < 0x10; ++column) {
					ImGui::TableSetColumnIndex(column + 1);
					ImGui::TextUnformatted("----");
				}

			}
			else {

				int column = 0;
				char row_label[7];
				char cell_label[5];

				// First row
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				std::snprintf(row_label, sizeof(row_label), "0x%04X", start_aligned);
				ImGui::TextUnformatted(row_label);
				for (column; column < off_start; ++column) {
					ImGui::TableSetColumnIndex(column + 1);
					ImGui::TextUnformatted("----");
				}
				for (column; column < 0x10; ++column) {
					ImGui::TableSetColumnIndex(column + 1);
					std::snprintf(cell_label, sizeof(cell_label), "0x%02X", ctx->vram.view[start_aligned + column]);
					ImGui::TextUnformatted(cell_label);
				}

				// Intermediate rows
				for (int row = 1; row < row_count - 1; ++row) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					std::snprintf(row_label, sizeof(row_label), "0x%04X", start_aligned + (row << 4));
					ImGui::TextUnformatted(row_label);
					column = 0;
					for (column; column < 0x10; ++column) {
						ImGui::TableSetColumnIndex(column + 1);
						std::snprintf(cell_label, sizeof(cell_label), "0x%02X", ctx->vram.view[start_aligned + (row << 4) + column]);
						ImGui::TextUnformatted(cell_label);
					}
				}

				// Last row
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				std::snprintf(row_label, sizeof(row_label), "0x%04X", end_aligned & ~0x000F);
				ImGui::TextUnformatted(row_label);
				column = 0;
				for (column; column < (0xF - off_end) + 1; ++column) {
					ImGui::TableSetColumnIndex(column + 1);
					std::snprintf(cell_label, sizeof(cell_label), "0x%02X", ctx->vram.view[(end_aligned & ~0x000F) + column]);
					ImGui::TextUnformatted(cell_label);
				}
				for (column; column < 0x10; ++column) {
					ImGui::TableSetColumnIndex(column + 1);
					ImGui::TextUnformatted("----");
				}

			}

			if (go_to_button_pressed) {
				if (ctx->vram.look_for < ctx->vram.start || ctx->vram.look_for > ctx->vram.end) {
					ImGui::OpenPopup("memory-view-range-error");
				}
				else {
					u32 base = ctx->vram.look_for - start_aligned;
					int row = base >> 4;
					ImGui::SetScrollY(row_height * row);
				}
			}
			if (ImGui::BeginPopup("memory-view-range-error")) {
				char a_buf[36], b_buf[25];
				std::snprintf(a_buf, sizeof(a_buf), "The address 0x%04X is outside range", ctx->vram.look_for);
				ImGui::Text(a_buf);
				std::snprintf(b_buf, sizeof(b_buf), "Range: [0x%04X - 0x%04X]", ctx->vram.start, ctx->vram.end);
				ImGui::Text(b_buf);
				if (ImGui::Button("Ok")) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}

			ImGui::EndTable();
		}

		ImGui::End();
	}

	ImGui::End();
}

void rom_window(_gui_context::_rom* ctx, cpu* cpu) {

	ImGui::SetNextWindowSize(
		ImVec2{ (ctx->show_chr || ctx->show_prg) ? 860.f : 300.f, 275.f}
	);
	if (!ImGui::Begin("Rom", nullptr, ImGuiWindowFlags_NoResize)) {
		ImGui::End();
		return;
	}

	ImGui::SeparatorText("Load Rom");
	if (ImGui::Button("Open")) {
		IGFD::FileDialogConfig config;
		config.path = ".";
		ImGuiFileDialog::Instance()->OpenDialog("choose-file", "Choose Rom", ".nes,.txt", config);
	}
	if (ImGuiFileDialog::Instance()->Display("choose-file")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string s = ImGuiFileDialog::Instance()->GetFilePathName();
			ctx->game_path_opened = std::filesystem::path(s);
			std::vector<u8> x = read_file(s);
			ctx->game_opened = new cartridge(x);
			ctx->inserted = true;
			ctx->loaded = false;
		}

		ImGuiFileDialog::Instance()->Close();
	}

	if (ctx->inserted && !ctx->loaded) {
		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
		if (ImGui::Button("Load")) {
			ctx->game_loaded = new cartridge(*ctx->game_opened);
			ctx->game_path_loaded = ctx->game_path_opened;
			cpu->load(ctx->game_loaded);
			
			cpu->reset();
			ctx->loaded = true;
			
		}
		ImGui::Text("Rom name: %s", ctx->game_path_opened.filename().string().c_str());
	}

	ImGui::SeparatorText("Currently loaded Rom");
	/*
	if (ctx->game_loaded != nullptr) {
		ImGui::Text("Name: %s", ctx->game_path_loaded.filename().string().c_str());
		ImGui::Text("Mapper: %d", static_cast<int>(ctx->game_loaded->mapper));
		int mirroring = ctx->game_loaded->screen_mirroring;
		ImGui::Text("Screen mirroring: %d (%s)", mirroring, cartridge::SCREEN_MIRRORING_NAMES[mirroring]);

		if (ctx->show_prg = ImGui::TreeNode("Prg content")) {

			usize row_count = ctx->game_loaded->prg_rom.size() / 16;
			int height_multiplier = row_count > 2 ? 4 : (row_count > 1 ? 3 : 2);
			float row_height = ImGui::GetTextLineHeightWithSpacing();

			ImVec2 outer_size = ImVec2(0.0f, row_height * height_multiplier);
			if (
				ImGui::BeginTable(
					"prg-rom-table",
					17,
					ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
					outer_size
				)
				) {
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableSetupColumn("v + >");
				for (int i = 0x0; i <= 0xF; ++i) {
					char label[5];
					std::snprintf(label, sizeof(label), "0x%02X", i);
					ImGui::TableSetupColumn(label);
				}
				ImGui::TableHeadersRow();
				int column = 0;
				char row_label[7];
				char cell_label[5];
				// Intermediate rows
				for (int row = 0; row < row_count; ++row) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					std::snprintf(row_label, sizeof(row_label), "0x%04X", (row << 4));
					ImGui::TextUnformatted(row_label);
					column = 0;
					for (column; column < 0x10; ++column) {
						ImGui::TableSetColumnIndex(column + 1);
						std::snprintf(cell_label, sizeof(cell_label), "0x%02X", ctx->game_loaded->prg_rom[(row << 4) + column]);
						ImGui::TextUnformatted(cell_label);
					}
				}

				ImGui::EndTable();
			}
			ImGui::TreePop();
		}
		if (ctx->show_chr = ImGui::TreeNode("Chr cnotent")) {

			usize row_count = ctx->game_loaded->chr_rom.size() / 16;
			int height_multiplier = row_count > 2 ? 4 : (row_count > 1 ? 3 : 2);
			float row_height = ImGui::GetTextLineHeightWithSpacing();

			ImVec2 outer_size = ImVec2(0.0f, row_height * height_multiplier);
			if (
				ImGui::BeginTable(
					"chr-rom-table",
					17,
					ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
					outer_size
				)
				) {
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableSetupColumn("v + >");
				for (int i = 0x0; i <= 0xF; ++i) {
					char label[5];
					std::snprintf(label, sizeof(label), "0x%02X", i);
					ImGui::TableSetupColumn(label);
				}
				ImGui::TableHeadersRow();
				int column = 0;
				char row_label[7];
				char cell_label[5];
				// Intermediate rows
				for (int row = 0; row < row_count; ++row) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					std::snprintf(row_label, sizeof(row_label), "0x%04X", (row << 4));
					ImGui::TextUnformatted(row_label);
					column = 0;
					for (column; column < 0x10; ++column) {
						ImGui::TableSetColumnIndex(column + 1);
						std::snprintf(cell_label, sizeof(cell_label), "0x%02X", ctx->game_loaded->chr_rom[(row << 4) + column]);
						ImGui::TextUnformatted(cell_label);
					}
				}

				ImGui::EndTable();
			}
			ImGui::TreePop();
		}
	}*/

	ImGui::End();
}

void cpu_window(_gui_context::_cpu* ctx, cpu* CPU) {

	float height = 85.f + (ctx->register_view ? 110.f : 0.f) + (ctx->instruction_view ? 90.f : 0.f) + (ctx->stack_view ? 30.f : 0.f);
	ImGui::SetNextWindowSize(ImVec2(435.f, height));
	if (!ImGui::Begin("CPU", nullptr, ImGuiWindowFlags_NoResize)) {
		ImGui::End();
		return;
	}
	ImGui::SeparatorText("CPU Status");
	if (CPU->halted.load()) {
		ImGui::Text("Halted ");
		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
		ImGui::PlotLines("CPU Frequency", { 0 }, 1, 0, nullptr, -1.f, 1.f, ImVec2(100.f, 20.f));
		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
		if (ImGui::Button("Reset")) {
			CPU->reset();
			ctx->has_started = false;
			update_cpu_registers(ctx, CPU);
		}
	}
	else {
		if (CPU->paused.load()) {
			ImGui::Text("Paused ");
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
			float freq[1] = { ctx->last_freq };
			ImGui::PlotLines("CPU Frequency", freq, 1, 0, nullptr, -1.f, 1.f, ImVec2(100.f, 20.f));
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();

			ImGui::BeginDisabled(!CPU->rom_loaded);
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
							update_cpu_registers(ctx, &cpu);
							update_cpu_instruction(ctx, &cpu);
						}
					);
					ctx->has_started = true;
				}
			}
			ImGui::EndDisabled();

		}
		else {
			ImGui::Text("Running");
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
			float freq[20];
			for (uint8_t n = 0; n < 20; n++) freq[n] = sinf(n * 5.f + static_cast<float>(ImGui::GetTime()) * 5.f);
			ImGui::PlotLines("CPU Frequency", freq, 20, 0, nullptr, -1.f, 1.f, ImVec2(100.f, 20.f));
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
				ImGui::Text("A:    0x%02X", ctx->registers.a);
				ImGui::Text("X:    0x%02X", ctx->registers.x);
				ImGui::Text("Y:    0x%02X", ctx->registers.y);
				ImGui::EndGroup();
			}
			ImGui::SameLine();
			{
				ImGui::BeginGroup();
				ImGui::Text("PC: 0x%04X", ctx->registers.pc);
				ImGui::Text("SP:   0x%02X", ctx->registers.sp);
				ImGui::Text("P:    0x%02X", ctx->registers.p);
				ImGui::EndGroup();
			}
		}
		ImGui::EndChild();

		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();

		// Status flags - Shows each flag in the P register
		ImGui::BeginChild("status", ImVec2(0.f, 65.f));
		{
			ImGui::SeparatorText("Status (P) flags");
			u8 p = ctx->registers.p;
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

		if (ctx->stack_view = ImGui::TreeNode("Stack Content")) {
			ImGui::BeginChild("stack-view", ImVec2(400.f, 30.f), ImGuiWindowFlags_HorizontalScrollbar);
			for (u8 s = 0xFD; s >= ctx->registers.sp; --s) {
				ImGui::BeginGroup();
					ImGui::Text("0x%02X", s);
					ImGui::Text("0x%02X", ctx->wram.view[s | 0x1000]);
				ImGui::EndGroup();
				ImGui::SameLine();
			}
			ImGui::EndChild();
			ImGui::TreePop();
		}

		// End - Register View
	}
	if (ctx->instruction_view) {
		// Start - Instruction View
		ImGui::SeparatorText("Current Instruction");
		ImGui::BeginChild("instruction-panel");

		// pc, buttons (fetch, decode, execute)
		ImGui::Text("PC: 0x%04X", ctx->registers.pc);
		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();

		ImGui::BeginDisabled(!CPU->rom_loaded || CPU->halted.load() || !CPU->paused.load());
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
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
			ImGui::Text("Cumulated cycles: %d", ctx->cycles);

			ImGui::Spacing();
			ImGui::Text("Addressing Mode: %s", ADDRESSING_MODE_NAMES[_instr->mode]);
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
			ImGui::Text("Bytes:  %d", mode.length);
			ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
			ImGui::Text("Cycles: %d", _instr->cycles);
		}

		ImGui::EndChild();
		// End - Instruction View
	}
	if (ctx->wram.show) {
		if (!ImGui::Begin("Memory")) {
			ImGui::End();
			ImGui::End();
			return;
		}

		ImGui::SeparatorText("Address Range");
		ImGui::SetNextItemWidth(150.f);
		InputHex("Start", &ctx->wram.start, 1, 16, 4);
		ctx->wram.start = static_cast<u32>(std::clamp(static_cast<u16>(ctx->wram.start & 0x0000FFFF), 0_u16, ctx->wram.max));

		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); ImGui::SetNextItemWidth(100.f);
		InputHex("Addr##", &ctx->wram.look_for, 1, 16, 4);
		ImGui::SameLine(); 
		char go_buf[13];
		std::snprintf(go_buf, sizeof(go_buf), "Go to 0x%04X", ctx->wram.look_for);
		bool go_to_button_pressed = ImGui::Button(go_buf);
		

		ImGui::SetNextItemWidth(150.f);
		InputHex("End", &ctx->wram.end, 1, 16, 4);
		ctx->wram.end = static_cast<u32>(std::clamp(static_cast<u16>(ctx->wram.end & 0x0000FFFF), static_cast<u16>(ctx->wram.start & 0x0000FFFF), ctx->wram.max));

		ImGui::SeparatorText("Memory View");

		int start_aligned = ctx->wram.start & ~0x000F;
		int end_aligned = ctx->wram.end | 0x000F;
		int element_count = 1 + end_aligned - start_aligned;
		int row_count = element_count / 16;

		int height_multiplier = row_count > 2 ? 4 : (row_count > 1 ? 3 : 2);
		float row_height = ImGui::GetTextLineHeightWithSpacing();
		ImVec2 outer_size = ImVec2(0.0f, row_height * height_multiplier);
		if (
			ImGui::BeginTable(
				"wram-table", 
				17, 
				ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
				outer_size
			)
		) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("v + >");
			for (int i = 0x0; i <= 0xF; ++i) {
				char label[5];
				std::snprintf(label, sizeof(label), "0x%02X", i);
				ImGui::TableSetupColumn(label);
			}
			ImGui::TableHeadersRow();

			/* If off_start, start with ---- on the first row
			 * If off_end, end with ---- on the last row
			 * 
			 * But, if start_aligned == end_aligned & ~0x000F, same row
			 */
			int off_start = ctx->wram.start - start_aligned;
			int off_end = end_aligned - ctx->wram.end;
			if (start_aligned == (end_aligned & ~0x000F)) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				char row_label[7];
				std::snprintf(row_label, sizeof(row_label), "0x%04X", start_aligned);
				ImGui::TextUnformatted(row_label);
				int column = 0;

				for (column; column < off_start; ++column) {
					ImGui::TableSetColumnIndex(column + 1);
					ImGui::TextUnformatted("----");
				}
				char cell_label[5];
				
				if (!CPU->halted.load() && CPU->paused.load()) {
					for (column; column < (0xF - off_end) + 1; ++column) {
						ImGui::TableSetColumnIndex(column + 1);
						std::snprintf(cell_label, sizeof(cell_label), "0x%02X", ctx->wram.view[start_aligned + column]);
						ImGui::TextUnformatted(cell_label);
					}
				}
				for (column; column < 0x10; ++column) {
					ImGui::TableSetColumnIndex(column + 1);
					ImGui::TextUnformatted("----");
				}

			}
			else {
			
				int column = 0;
				char row_label[7];
				char cell_label[5];

				// First row
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				std::snprintf(row_label, sizeof(row_label), "0x%04X", start_aligned);
				ImGui::TextUnformatted(row_label);
				for (column; column < off_start; ++column) {
					ImGui::TableSetColumnIndex(column + 1);
					ImGui::TextUnformatted("----");
				}
				for (column; column < 0x10; ++column) {
					ImGui::TableSetColumnIndex(column + 1);
					std::snprintf(cell_label, sizeof(cell_label), "0x%02X", ctx->wram.view[start_aligned + column]);
					ImGui::TextUnformatted(cell_label);
				}
				
				// Intermediate rows
				for (int row = 1; row < row_count - 1; ++row) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					std::snprintf(row_label, sizeof(row_label), "0x%04X", start_aligned + (row << 4));
					ImGui::TextUnformatted(row_label);
					column = 0;
					for (column; column < 0x10; ++column) {
						ImGui::TableSetColumnIndex(column + 1);
						std::snprintf(cell_label, sizeof(cell_label), "0x%02X", ctx->wram.view[start_aligned + (row << 4) + column]);
						ImGui::TextUnformatted(cell_label);
					}
				}

				// Last row
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				std::snprintf(row_label, sizeof(row_label), "0x%04X", end_aligned & ~0x000F);
				ImGui::TextUnformatted(row_label);
				column = 0;
				for (column; column < (0xF - off_end) + 1; ++column) {
					ImGui::TableSetColumnIndex(column + 1);
					std::snprintf(cell_label, sizeof(cell_label), "0x%02X", ctx->wram.view[(end_aligned & ~0x000F) + column]);
					ImGui::TextUnformatted(cell_label);
				}
				for (column; column < 0x10; ++column) {
					ImGui::TableSetColumnIndex(column + 1);
					ImGui::TextUnformatted("----");
				}

			}

			if (go_to_button_pressed) {
				if (ctx->wram.look_for < ctx->wram.start || ctx->wram.look_for > ctx->wram.end) {
					ImGui::OpenPopup("memory-view-range-error");
				}
				else {
					u32 base = ctx->wram.look_for - start_aligned;
					int row = base >> 4;
					ImGui::SetScrollY(row_height * row);
				}
			}
			if (ImGui::BeginPopup("memory-view-range-error")) {
				char a_buf[36], b_buf[25];
				std::snprintf(a_buf, sizeof(a_buf), "The address 0x%04X is outside range", ctx->wram.look_for);
				ImGui::Text(a_buf);
				std::snprintf(b_buf, sizeof(b_buf), "Range: [0x%04X - 0x%04X]", ctx->wram.start, ctx->wram.end);
				ImGui::Text(b_buf);
				if (ImGui::Button("Ok")) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}

			ImGui::EndTable();
		}

		ImGui::End();
	}
	ImGui::End();
}

inline void static gui_fetch(_gui_context::_cpu* ctx, cpu* CPU) {
	CPU->fetch();
	ctx->opcode.fetched = true;
	ctx->opcode.executed = false;

	update_cpu_registers(ctx, CPU);
}
inline void static gui_decode(_gui_context::_cpu* ctx, cpu* CPU) {
	CPU->decode();

	update_cpu_instruction(ctx, CPU);
}
inline void static gui_execute(_gui_context::_cpu* ctx, cpu* CPU) {
	CPU->execute();
	ctx->opcode.fetched = false;
	ctx->opcode.executed = true;

	update_cpu_registers(ctx, CPU);
}