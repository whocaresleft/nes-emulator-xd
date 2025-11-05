#ifndef CPU__H
#define CPU__H

#include "bus.h"
#include <string>
#include "interrupt.h"
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <future>
#include <stop_token>

struct instruction;
struct addressing_mode;

class cpu
{
private:
	constexpr static u16 STACK = 0x0100_u16;
	constexpr static u8 STACK_RESET = 0xFD_u8;

	constexpr static u8 F_CARRY					= 0b00000001;
	constexpr static u8 F_ZERO					= 0b00000010;
	constexpr static u8 F_INTERRUPT_DISABLE		= 0b00000100;
	constexpr static u8 F_DECIMAL_MODE			= 0b00001000;
	constexpr static u8 F_BREAK					= 0b00010000;
	constexpr static u8 F_GHOST					= 0b00100000;
	constexpr static u8 F_OVERFLOW				= 0b01000000;
	constexpr static u8 F_NEGATIVE				= 0b10000000;

	u8 a, p, sp, x, y;
	u16 pc;

	u8 opcode;

	usize cycles;

	std::atomic<bool> halted;
	std::atomic<bool> paused;
	std::atomic<bool> nmi_requested;

	bool rom_loaded;
	const instruction* decoded;

	std::mutex mtx;
	std::condition_variable cv;
	std::jthread runner;

	bus* cpu_bus;

	void tick(const usize cycles) { 
		this->cycles += cycles; 
		this->cpu_bus->tick(cycles);
	}

	void set_a(const u8 value) { this->a = value; this->update_negative_zero(this->a); }
	void add_to_a(const u8 value);
	void branch(const bool condition);
	void compare(const addressing_mode mode, const u8 compare_with);

	void set_carry(const bool value) { value ? this->p |= F_CARRY : this->p &= ~F_CARRY; }
	void set_zero(const bool value) { value ? this->p |= F_ZERO : this->p &= ~F_ZERO; }
	void set_interrupt_disable(const bool value) { value ? this->p |= F_INTERRUPT_DISABLE : this->p &= ~F_INTERRUPT_DISABLE; }
	void set_decimal_mode(const bool value) { value ? this->p |= F_DECIMAL_MODE : this->p &= ~F_DECIMAL_MODE; }
	void set_break(const bool value) { value ? this->p |= F_BREAK : this->p &= ~F_BREAK; }
	void set_overflow(const bool value) { value ? this->p |= F_OVERFLOW : this->p &= ~F_OVERFLOW; }
	void set_negative(const bool value) { value ? this->p |= F_NEGATIVE : this->p &= ~F_NEGATIVE; }

	bool get_carry() { return 0 != (this->p & F_CARRY); }
	bool get_zero() { return 0 != (this->p & F_ZERO); }
	bool get_interrupt_disable() { return 0 != (this->p & F_INTERRUPT_DISABLE); }
	bool get_decimal_mode() { return 0 != (this->p & F_DECIMAL_MODE); }
	bool get_break() { return 0 != (this->p & F_BREAK); }
	bool get_overflow() { return 0 != (this->p & F_OVERFLOW); }
	bool get_negative() { return 0 != (this->p & F_NEGATIVE); }

	void update_negative_zero(const u8 value) {
		this->set_zero(value == 0_u8);
		this->set_negative(0_u8 != (value & 0b10000000));
		return;
	}

public:
	std::span<u8> get_wram() { return this->cpu_bus->get_wram(); }
	u8 get_a() const { return this->a; }
	u8 get_x() const { return this->x; }
	u8 get_y() const { return this->y; }
	u8 get_p() const { return this->p; }
	u16 get_pc() const { return this->pc; }
	u8 get_sp() const { return this->sp; }
	u8 get_opcode() const { return this->opcode; }
	usize get_cycles() const { return this->cycles; }
	bool get_halted() const { return this->halted.load(); }
	bool get_paused() const { return this->paused.load(); }
	bool get_rom_loaded() const { return this->rom_loaded; }
	const instruction* get_decoded() { return this->decoded; }

	void request_nmi() { this->nmi_requested.store(true); }
	bool is_nmi_requested() const { return this->nmi_requested.load(); }

	std::pair<u16, bool> get_absolute_address(const u16 address);
	std::pair<u16, bool> get_absolute_x_address(const u16 address);
	std::pair<u16, bool> get_absolute_y_address(const u16 address);
	std::pair<u16, bool> get_immediate_address(const u16 address);
	std::pair<u16, bool> get_indirect_x_address(const u16 address);
	std::pair<u16, bool> get_indirect_y_address(const u16 address);
	std::pair<u16, bool> get_zero_page_address(const u16 address);
	std::pair<u16, bool> get_zero_page_x_address(const u16 address);
	std::pair<u16, bool> get_zero_page_y_address(const u16 address);

	void ldy(const addressing_mode mode);
	void axs(const addressing_mode mode);
	void inc(const addressing_mode mode);
	void las(const addressing_mode mode);
	void php(const addressing_mode mode);
	void lsr_accumulator(const addressing_mode mode);
	void lsr(const addressing_mode mode);
	void tsx(const addressing_mode mode);
	void bcc(const addressing_mode mode);
	void asl(const addressing_mode mode);
	void rla(const addressing_mode mode);
	void and_ (const addressing_mode mode);
	void alr(const addressing_mode mode);
	void arr(const addressing_mode mode);
	void asl_accumulator(const addressing_mode mode);
	void sty(const addressing_mode mode);
	void ldx(const addressing_mode mode);
	void txs(const addressing_mode mode);
	void sei(const addressing_mode mode);
	void bcs(const addressing_mode mode);
	void lda(const addressing_mode mode);
	void tay(const addressing_mode mode);
	void rol_accumulator(const addressing_mode mode);
	void iny(const addressing_mode mode);
	void eor(const addressing_mode mode);
	void cli(const addressing_mode mode);
	void brk(const addressing_mode mode);
	void rol(const addressing_mode mode);
	void clc(const addressing_mode mode);
	void bvc(const addressing_mode mode);
	void shx(const addressing_mode mode);
	void inx(const addressing_mode mode);
	void isc(const addressing_mode mode);
	void pha(const addressing_mode mode);
	void cld(const addressing_mode mode);
	void bit(const addressing_mode mode);
	void anc(const addressing_mode mode);
	void jmp_ind(const addressing_mode mode);
	void sta(const addressing_mode mode);
	void sre(const addressing_mode mode);
	void ror_accumulator(const addressing_mode mode);
	void dex(const addressing_mode mode);
	void bvs(const addressing_mode mode);
	void tas(const addressing_mode mode);
	void adc(const addressing_mode mode);
	void dec(const addressing_mode mode);
	void sax(const addressing_mode mode);
	void lax(const addressing_mode mode);
	void dey(const addressing_mode mode);
	void stx(const addressing_mode mode);
	void ror(const addressing_mode mode);
	void jsr(const addressing_mode mode);
	void stp(const addressing_mode mode);
	void sed(const addressing_mode mode);
	void rti(const addressing_mode mode);
	void rra(const addressing_mode mode);
	void bmi(const addressing_mode mode);
	void pla(const addressing_mode mode);
	void sbc(const addressing_mode mode);
	void clv(const addressing_mode mode);
	void tya(const addressing_mode mode);
	void sec(const addressing_mode mode);
	void txa(const addressing_mode mode);
	void dcp(const addressing_mode mode);
	void cpy(const addressing_mode mode);
	void slo(const addressing_mode mode);
	void ora(const addressing_mode mode);
	void tax(const addressing_mode mode);
	void jmp_abs(const addressing_mode mode);
	void rts(const addressing_mode mode);
	void beq(const addressing_mode mode);
	void plp(const addressing_mode mode);
	void shy(const addressing_mode mode);
	void cpx(const addressing_mode mode);
	void nop_implied(const addressing_mode mode);
	void nop(const addressing_mode mode);
	void ahx(const addressing_mode mode);
	void bpl(const addressing_mode mode);
	void bne(const addressing_mode mode);
	void cmp(const addressing_mode mode);
	void xaa(const addressing_mode mode);

	cpu() :
		rom_loaded(false),
		opcode(0x00_u8),
		decoded(nullptr),

		pc(0x0000_u16),
		sp(STACK_RESET),
		a(0x00_u8),
		x(0x00_u8),
		y(0x00_u8),
		p(0x00_u8 | F_INTERRUPT_DISABLE | F_BREAK | F_GHOST),
		cycles(7_usize),
		halted(false),
		paused(true),
		nmi_requested(0),

		cpu_bus(nullptr)
	{
		//this->pc = 0xc000; // for testnes
		//this->pc = this->read_u16(0xFFFC);
	}
	cpu(cpu& to_copy) = delete;
	cpu(cpu&& to_move) noexcept = delete;

	void connect(bus* cpu_bus) { this->cpu_bus = cpu_bus; }

	void load(cartridge* rom) { 
		this->cpu_bus->load(rom);
		this->rom_loaded = true;
		this->pc = this->read_u8(0xFFFC);
	}
	void reset() {
		this->opcode = 0x00_u8;
		this->decoded = nullptr;
		this->sp -= 3;
		//this->a = 0x00_u8;
		//this->x = 0x00_u8;
		//this->y = 0x00_u8;
		this->p |= F_INTERRUPT_DISABLE;
		this->cycles = 7_usize;
		this->halted = false;
		this->paused = true;
		this->pc = this->read_u16(0xFFFC_u16);
		//this->pc = 0xC000; // for testnes
	}

	u8 read_u8(const u16 address) { return this->cpu_bus->read_u8(address); }
	void write_u8(const u16 address, const u8 value) { this->cpu_bus->write_u8(address, value); }
	u16 read_u16(const u16 address) { return this->cpu_bus->read_u16(address); }
	void write_u16(const u16 address, const u16 value) { this->cpu_bus->write_u16(address, value); }

	u8 pop_u8() { return this->read_u8(STACK | static_cast<u16>(++this->sp)); }
	u16 pop_u16() { return static_cast<u16>(this->pop_u8()) | static_cast<u16>((this->pop_u8()) << 8); }
	void push_u8(const u8 value) { this->write_u8(STACK | static_cast<u16>(this->sp--), value); }
	void push_u16(const u16 value) {
		this->push_u8(static_cast<u8>((value & 0xFF00) >> 8));
		this->push_u8(static_cast<u8>(value & 0x00FF));
	}

	void run() { this->run_with_callbacks([](cpu&) {}, [](cpu&) {}); }
	template <typename F> void run_with_callback_first(F&& first) { this->run_with_callbacks(first, [](cpu&) {}); }
	template <typename L> void run_with_callback_last(L&& last) { this->run_with_callbacks([](cpu&) {}, last); }
	template <typename F, typename L>
	void run_with_callbacks(F&& first, L&& last) {
		while (!this->halted) {
			first(*this);

			if (this->nmi_requested.load()) this->handle_nmi();
			this->fetch();
			this->decode();
			this->execute();

			last(*this);
		}
	}

	void run_async() { this->run_async([](cpu&) {}, [](cpu&) {}); }
	template <typename F, typename L>
	void run_async(F&& first, L&& last) {
		if (halted.load()) return;
		this->paused.store(false);

		runner = std::jthread([this, first, last](std::stop_token st) {

			while (!st.stop_requested()) {
				{
					std::unique_lock<std::mutex> lock(mtx);
					cv.wait(
						lock, [this, &st] {
							return st.stop_requested() || (!paused && !halted);
						}
					);
				}
				if (st.stop_requested()) break;
				
				first(*this);
				if (this->nmi_requested.load()) this->handle_nmi();
				this->fetch();
				this->decode();
				this->execute();
				last(*this);

				if (halted.load()) break;
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
			}
		});
	}
	void pause() {
		if (halted.load()) return;
		paused.store(true);
	}
	void resume() {
		if (!paused.load() || halted.load()) return;
		{
			std::lock_guard<std::mutex> lock(mtx);
			paused.store(false);
		}
		cv.notify_one();
	}

	std::string trace();

	~cpu() { this->halted.store(true); }

	void fetch() {
		this->opcode = this->read_u8(this->pc++);
		//std::cout << std::hex << "Fetched " << static_cast<int>(this->opcode) << " at " << static_cast<int>(this->pc - 1) << std::dec << std::endl;
	}
	void decode();
	void execute();

	void interrupt(const interrupt *cpu_int) {
		this->push_u16(this->pc);
		this->push_u8(this->p | F_GHOST | (cpu_int->b_g_mask & F_BREAK));
		this->set_interrupt_disable(true);
		this->tick(cpu_int->cpu_cycles);
		this->pc = this->read_u16(cpu_int->vector_address);
	}

	void handle_nmi() { this->nmi_requested.store(false); this->interrupt(&interrupts::nmi_interrupt); }
	void handle_irq() { this->interrupt(&interrupts::irq_interrupt); }
};

#include "instruction.h"
extern const addressing_mode ADDRESSING_MODES[13];
extern const instruction INSTRUCTIONS[0x100];

#endif