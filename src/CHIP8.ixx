export module CHIP8;

import NumericalTypes;
import SerializationStream;

import <algorithm>;
import <array>;
import <cstdlib>;
import <format>;
import <optional>;
import <string>;
import <utility>;
import <vector>;

namespace CHIP8
{
	export
	{
		enum class Key {
			k0, k1, k2, k3, k4, k5, k6, k7, k8, k9, kA, kB, kC, kD, kE, kF
		};

		void ApplyNewSampleRate();
		bool AssociatesWithRomExtension(const std::string& ext);
		void Detach();
		void DisableAudio();
		void EnableAudio();
		uint GetNumberOfInputs();
		void Initialize();
		bool LoadBios(const std::string& path);
		bool LoadRom(const std::string& path);
		void NotifyNewAxisValue(uint player_index, uint input_action_index, int axis_value);
		void NotifyButtonPressed(uint player_index, uint button_index);
		void NotifyButtonReleased(uint player_index, uint button_index);
		void Reset();
		void Run();
		void SetAudioEffect(uint index);
		void SetNumInstructionsPerSecond(uint number);
		void StreamState(SerializationStream& stream);
	}

	void DecodeAndExecuteInstruction(u16 opcode);
	void StepCycle();
	void UpdateTimers();

	constexpr uint resolution_x = 64;
	constexpr uint resolution_y = 32;
	constexpr uint framebuffer_size = resolution_x * resolution_y;
	constexpr uint rom_start_addr = 0x200;

	/* Mark this as constexpr, and MSVC will freak out */
	std::array sound_effect_paths = {
		"audio/beep-01.wav",
		"audio/beep-02.wav",
		"audio/beep-03.wav"
	};

	bool audio_enabled;

	u8 delay_timer;
	u8 sound_timer;

	u16 index; // index register
	u16 pc; // program counter
	u16 sp; // stack pointer

	uint instructions_per_second;
	uint sound_effect_index;

	std::array<u8, framebuffer_size> framebuffer;
	std::array<bool, 0x10> key;
	std::array<u8, 0x1000> memory;
	std::array<u8, 0x10> stack;
	std::array<u8, 0x10> v; // general-purpose registers
}