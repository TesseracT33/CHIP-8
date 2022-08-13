module CHIP8;

import Audio;
import Cores;
import Input;
import UserMessage;
import Video;

import Util.Files;

namespace CHIP8
{
	void ApplyNewSampleRate()
	{
		/* does not apply */
	}


	bool AssociatesWithRomExtension(const std::string& ext)
	{
		return ext.compare("ch8") == 0 || ext.compare("chip8") == 0
			|| ext.compare("CH8") == 0 || ext.compare("CHIP8") == 0;
	}


	void DecodeAndExecuteInstruction(u16 opcode)
	{
		auto GetX = [opcode] { /* opcode pattern nxnn */
			return opcode >> 8 & 0xF;
		};
		auto GetY = [opcode] { /* opcode pattern nnyn */
			return opcode >> 4 & 0xF;
		};

		switch (opcode >> 12) {
		case 0:
			if (opcode == 0x00E0) { /* 00E0; CLS -- Clear the display. */
				framebuffer.fill(0);
			}
			else if (opcode == 0x00EE) { /* 00EE; RET -- Return from subroutine. */
				pc = stack[sp--];
			}
			else {
				UserMessage::Show(std::format("Unknown opcode {} encountered.", opcode), UserMessage::Type::Fatal);
			}
			break;

		case 1: /* 1nnn; JP addr -- Jump to location nnn. */
			pc = opcode & 0xFFF;
			break;

		case 2: /* 2nnn; CALL addr -- Call subroutine at nnn. */
			stack[++sp] = pc & 0xFF;
			pc = opcode & 0xFFF;
			break;

		case 3: /* 3xkk; SE Vx, byte -- Skip next instruction if Vx = kk. */
			if (v[GetX()] == (opcode & 0xFF)) {
				pc += 2;
			}
			break;

		case 4: /* 4xkk; SNE Vx, byte -- Skip next instruction if Vx != kk. */
			if (v[GetX()] != (opcode & 0xFF)) {
				pc += 2;
			}
			break;

		case 5: /* 5xy0; SE Vx, Vy -- Skip next instruction if Vx = Vy. */
			if (v[GetX()] == v[GetY()]) {
				pc += 2;
			}
			break;

		case 6: /* 6xkk; LD Vx, byte -- Set Vx = kk. */
			v[GetX()] = opcode & 0xFF;
			break;

		case 7: /* 7xkk; ADD Vx, byte -- Set Vx = Vx + kk. */
			v[GetX()] += opcode & 0xFF;
			break;

		case 8: {
			auto x = GetX();
			auto y = GetY();
			switch (opcode & 0xF) {
			case 0: /* 8xy0; LD Vx, Vy -- Set Vx = Vy. */
				v[x] = v[y];
				break;

			case 1: /* 8xy1; OR Vx, Vy -- Set Vx = Vx OR Vy. */
				v[x] |= v[y];
				break;

			case 2: /* 8xy2; AND Vx, Vy -- Set Vx = Vx AND Vy. */
				v[x] &= v[y];
				break;

			case 3: /* 8xy3; XOR Vx, Vy -- Set Vx = Vx XOR Vy. */
				v[x] ^= v[y];
				break;

			case 4: /* 8xy4; ADD Vx, Vy -- Set Vx = Vx + Vy, and set VF = carry. */
				v[0xF] = v[y] > 0xFF - v[x];
				v[x] += v[y];
				break;

			case 5: /* 8xy5; SUB Vx, Vy -- Set Vx = Vx - Vy, and set VF = NOT borrow. */
				v[0xF] = v[y] <= v[x];
				v[x] -= v[y];
				break;

			case 6: /* 8xy6; SHR Vx {, Vy} -- Set VF to the LSB of Vx, and set Vx = Vx SHR 1.  */
				v[0xF] = v[x] & 0x01;
				v[x] >>= 1;
				break;

			case 7: /* 8xy7; SUBN Vx, Vy -- Set Vx = Vy - Vx, and set VF = NOT borrow. */
				v[0xF] = v[y] >= v[x];
				v[x] = v[y] - v[x];
				break;

			case 0xE: /* 8xyE; SHL Vx {, Vy} -- Set VF to the MSB of Vx, and set Vx = Vx SHL 1. */
				v[0xF] = v[x] >> 7;
				v[x] <<= 1;
				break;

			default:
				UserMessage::Show(std::format("Unknown opcode {} encountered.", opcode), UserMessage::Type::Fatal);
				break;
			}
			break;
		}

		case 9: /* 9xy0; SNE Vx, Vy -- Skip next instruction if Vx != Vy. */
			if (v[GetX()] != v[GetY()]) {
				pc += 2;
			}
			break;

		case 0xA: /* Annn; LD I, addr -- Set I = nnn. */
			index = opcode & 0xFFF;
			break;

		case 0xB: /* Bnnn; JP V0, addr -- Jump to location nnn + v0 */
			pc = (opcode + v[0]) & 0xFFF;
			break;

		case 0xC: /* Cxkk; RND Vx, byte -- Set Vx = random byte AND kk. */
			v[GetX()] = std::rand() & opcode & 0xFF;
			break;

		case 0xD: { /* Dxyn; DRW Vx, Vy, nibble -- Display n-byte sprite starting at memory location I at (Vx, Vy), and set VF = collision. */
			auto vx = v[GetX()];
			auto vy = v[GetY()];
			auto height = opcode & 0xF;
			v[0xF] = 0;
			for (int yline = 0; yline < height; ++yline) {
				auto pixel = memory[(index + yline) & 0xFFF];
				for (int xline = 0; xline < 8; ++xline) {
					auto framebuffer_pos = (vx + xline + ((vy + yline) * 64)) % framebuffer_size;
					if ((pixel & (0x80 >> xline)) != 0) {
						if (framebuffer[framebuffer_pos]) {
							v[0xF] = 1;
						}
						framebuffer[framebuffer_pos] ^= 0xFF;
					}
				}
			}
			break;
		}

		case 0xE:
			if ((opcode & 0xFF) == 0x9E) { /* Ex9E; SKP Vx -- Skip next instruction if key with the value of Vx is pressed. */
				if (key[v[GetX()]]) {
					pc += 2;
				}
			}
			else if ((opcode & 0xFF) == 0xA1) { /* ExA1; SKNP Vx -- Skip next instruction if key with the value of Vx is not pressed. */
				if (!key[v[GetX()]]) {
					pc += 2;
				}
			}
			else {
				UserMessage::Show(std::format("Unknown opcode {} encountered.", opcode), UserMessage::Type::Fatal);
			}
			break;

		case 0xF:
			switch (opcode & 0xFF)
			{
			case 0x07: /* Fx07; LD Vx, DT -- Set Vx = delay timer value. */
				v[GetX()] = delay_timer;
				break;

			case 0x0A: { /* Fx0A; LD Vx, K --  Wait for a key press, store the value of the key in Vx. */
				std::array<bool, 0x10> prev_key = key;
				Input::Await(); /* Block until a key has been pressed */
				/* 'key' has now been mutated */
				for (size_t i = 0; i < key.size(); ++i) {
					if (key[i] != prev_key[i]) {
						v[GetX()] = i & 0xFF;
						break;
					}
				}
				break;
			}

			case 0x15: /* Fx15; LD DT, Vx -- Set delay timer = Vx */
				delay_timer = v[GetX()];
				break;

			case 0x18: /* Fx15; LD ST, Vx -- Set sound timer = Vx */
				sound_timer = v[GetX()];
				break;

			case 0x1E: /* Fx15; ADD I, Vx -- Set I = I + Vx */
				index += v[GetX()];
				break;

			case 0x29: /* Fx29; LD F, Vx -- Set I = location of sprite for digit Vx. */
				index = v[GetX()] * 5;
				break;

			case 0x33: { /* Fx33; LD B, Vx --  Store BCD representation of Vx in memory locations I, I+1, and I+2. */
				/* The interpreter takes the decimal value of Vx, and places the hundred's digit in memory at location in I,
				the ten's digit at location I+1, and the one's digit at location I+2. */
				auto x = GetX();
				memory[index] = v[x] / 100;
				memory[index + 1] = (v[x] / 10) % 10;
				memory[index + 2] = v[x] % 10;
				break;
			}

			case 0x55: { /* Fx55; LD [I], Vx -- Store registers V0 through Vx in memory starting at location I. */
				auto x = GetX();
				for (auto i = 0x0; i <= x; ++i) {
					memory[(index + i) & 0xFFF] = v[i];
				}
				break;
			}

			case 0x65: { /* Fx65; LD Vx, [I] -- Read registers V0 through Vx from memory starting at location I. */
				auto x = GetX();
				for (auto i = 0x0; i <= x; ++i) {
					v[i] = memory[(index + i) & 0xFFF];
				}
				break;
			}

			default:
				UserMessage::Show(std::format("Unknown opcode {} encountered.", opcode), UserMessage::Type::Fatal);
				break;
			}
			break;
		}
	}


	void Detach()
	{
		Audio::CloseFile();
	}


	void DisableAudio()
	{
		if (audio_enabled) {
			audio_enabled = false;
			Audio::CloseFile();
		}
	}


	void EnableAudio()
	{
		if (!audio_enabled) {
			audio_enabled = true;
			Audio::OpenFileForPlaying(sound_effect_paths[sound_effect_index]);
		}
	}


	uint GetNumberOfInputs()
	{
		return 16;
	}


	void Initialize()
	{
		Reset();
		memory.fill(0);
		//Audio::OpenFileForPlaying();
		Video::SetFramebufferPtr(framebuffer.data());
		Video::SetFramebufferSize(resolution_x, resolution_y);
		Video::SetPixelFormat(Video::PixelFormat::INDEX1LSB);
	}


	bool LoadBios(const std::string& path)
	{
		/* TODO */
		return false;
	}


	bool LoadRom(const std::string& path)
	{
		std::optional<std::vector<u8>> opt_rom = Util::Files::LoadBinaryFileVec(path);
		if (!opt_rom.has_value()) {
			UserMessage::Show(std::format("Could not open file at {}", path), UserMessage::Type::Error);
			return false;
		}
		std::vector<u8> rom = opt_rom.value();
		static constexpr auto max_rom_size = memory.size() - rom_start_addr;
		if (rom.size() > max_rom_size) {
			UserMessage::Show(std::format("Rom is {} bytes large, but must be at most {} bytes large.", rom.size(), max_rom_size), 
				UserMessage::Type::Error);
			return false;
		}
		std::copy(rom.begin(), rom.end(), memory.begin() + rom_start_addr);
		return true;
	}


	void NotifyNewAxisValue(uint player_index, uint input_action_index, int axis_value)
	{
		/* no axes */
	}


	void NotifyButtonPressed(uint player_index, uint button_index)
	{
		if (player_index == 0) {
			key[button_index] = 1;
		}
	}


	void NotifyButtonReleased(uint player_index, uint button_index)
	{
		if (player_index == 0) {
			key[button_index] = 0;
		}
	}


	void Reset()
	{
		delay_timer = sound_timer = 60;

		index = 0;
		pc = 0x200;
		sp = 0;

		framebuffer.fill(0);
		key.fill(0);
		stack.fill(0);
		v.fill(0);

		std::fill(memory.begin(), memory.begin() + rom_start_addr, 0); /* Do not overwrite the ROM area */

		static constexpr std::array<u8, 0x50> fontset = {
			0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
			0x20, 0x60, 0x20, 0x20, 0x70, // 1
			0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
			0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
			0x90, 0x90, 0xF0, 0x10, 0x10, // 4
			0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
			0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
			0xF0, 0x10, 0x20, 0x40, 0x40, // 7
			0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
			0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
			0xF0, 0x90, 0xF0, 0x90, 0x90, // A
			0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
			0xF0, 0x80, 0x80, 0x80, 0xF0, // C
			0xE0, 0x90, 0x90, 0x90, 0xE0, // D
			0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
			0xF0, 0x80, 0xF0, 0x80, 0x80  // F
		};
		std::copy(fontset.begin(), fontset.end(), memory.begin());
	}


	void Run()
	{
		/* Render at 60 fps. Timers are updated at 60 Hz. */
		const auto num_instructions = instructions_per_second / 60;
		for (uint i = 0; i < num_instructions; ++i) {
			StepCycle();
		}
		UpdateTimers();
		Video::RenderGame();
	}


	void SetAudioEffect(uint index)
	{
		if (index >= sound_effect_paths.size()) {
			UserMessage::Show(std::format("Could not load sound effect; index {} was specified, "
				"but the number of available sound effects is {}.", index, sound_effect_paths.size()),
				UserMessage::Type::Warning);
		}
		else if (index != sound_effect_index) {
			Audio::OpenFileForPlaying(sound_effect_paths[index]);
			sound_effect_index = index;
		}
	}


	void SetNumInstructionsPerSecond(uint number)
	{
		instructions_per_second = number;
	}


	void StepCycle()
	{
		u16 opcode = memory[pc] << 8 | memory[(pc + 1) & 0xFFF];
		pc = (pc + 2) & 0xFFF;
		DecodeAndExecuteInstruction(opcode);
	}


	void StreamState(SerializationStream& stream)
	{
		stream.StreamArray(framebuffer);
		stream.StreamArray(key);
		stream.StreamArray(memory);
		stream.StreamArray(stack);
		stream.StreamArray(v);
		stream.StreamPrimitive(delay_timer);
		stream.StreamPrimitive(delay_timer);
		stream.StreamPrimitive(index);
		stream.StreamPrimitive(pc);
		stream.StreamPrimitive(sp);
	}


	void UpdateTimers()
	{
		if (delay_timer > 0) {
			delay_timer--;
		}
		if (sound_timer > 0) {
			sound_timer--;
			Audio::PlayFile();
		}
	}
}