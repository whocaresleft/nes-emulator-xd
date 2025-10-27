#ifndef CPU__H
#define CPU__H

#include "bus.h"
#include <string>

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <future>
#include <stop_token>

struct instruction;
struct addressing_mode;


class cpu : public memory
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

public:
	u8 a, p, sp, x, y;
	u16 pc;

	u8 opcode;

	usize cycles;

	std::atomic<bool> halted;
	std::atomic<bool> paused;

	const instruction* decoded;
	std::span<u8> get_wram_chunk(usize first_inclusive, usize last_inclusive);
private:

	std::mutex mtx;
	std::condition_variable cv;
	std::jthread runner;

	bus cpu_bus;

	void tick(const usize cycles);

	void set_a(const u8 value);
	void add_to_a(const u8 value);
	void branch(const bool condition);
	void compare(const addressing_mode mode, const u8 compare_with);

	void set_carry(bool value);
	void set_zero(bool value);
	void set_interrupt_disable(bool value);
	void set_decimal_mode(bool value);
	void set_break(bool value);
	void set_overflow(bool value);
	void set_negative(bool value);

	bool get_carry();
	bool get_zero();
	bool get_interrupt_disable();
	bool get_decimal_mode();
	bool get_break();
	bool get_overflow();
	bool get_negative();

	void update_negative_zero(const u8 value);
public:
	
	std::pair<u16, bool> get_absolute_address(const u16 address) const;
	std::pair<u16, bool> get_absolute_x_address(const u16 address) const;
	std::pair<u16, bool> get_absolute_y_address(const u16 address) const;
	std::pair<u16, bool> get_immediate_address(const u16 address) const;
	std::pair<u16, bool> get_indirect_x_address(const u16 address) const;
	std::pair<u16, bool> get_indirect_y_address(const u16 address) const;
	std::pair<u16, bool> get_zero_page_address(const u16 address) const;
	std::pair<u16, bool> get_zero_page_x_address(const u16 address) const;
	std::pair<u16, bool> get_zero_page_y_address(const u16 address) const;

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

	cpu();
	cpu(cpu& to_copy) = delete;
	cpu(cpu&& to_move) noexcept = delete;

	void load(cartridge* rom);
	void reset();

	u8 read_u8(const u16 address) const override;
	void write_u8(const u16 address, const u8 value) override;

	u8 pop_u8();
	u16 pop_u16();
	void push_u8(const u8 value);
	void push_u16(const u16 value);
	

	void run();
	template <typename F>
	void run_with_callback_first(F&& first) {
		this->run_with_callbacks(
			first,
			[](cpu&) {}
		);
	}
	template <typename L>
	void run_with_callback_last(L&& last) {
		this->run_with_callbacks(
			[](cpu&) {},
			last
		);
	}
	template <typename F, typename L>
	void run_with_callbacks(F&& first, L&& last) {
		while (!this->halted) {
			first(*this);

			this->fetch();
			this->decode();
			this->execute();

			last(*this);
		}
	}

	void run_async() {
		this->run_async(
			[](cpu&) {},
			[](cpu&) {}
		);
	}
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
				this->fetch();
				this->decode();
				this->execute();
				last(*this);

				if (halted.load()) break;
				//std::this_thread::sleep_for(std::chrono::seconds(1));
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

	std::string trace() const;

	~cpu();

	void fetch();
	void decode();
	void execute();
};

#include "instruction.h"
extern const addressing_mode ADDRESSING_MODES[13];
extern const instruction INSTRUCTIONS[0x100];

#endif