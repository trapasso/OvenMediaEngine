#include "mpegts_pes.h"

#define OV_LOG_TAG	"MpegTsPes"

namespace mpegts
{
	Pes::Pes(uint16_t pid)
	{
		_pid = pid;
	}

	Pes::~Pes()
	{

	}

	// return consumed length
	size_t Pes::AppendData(const uint8_t *data, uint32_t length)
	{
		// it indicates how many bytes are used in this function
		// it means current position in data and remains length - consumed_length
		size_t consumed_length = 0;

		if(_pes_header_parsed == false)
		{
			// Parsing header first
			auto current_length = _data.GetLength();
			auto need_length = static_cast<size_t>(MPEGTS_PES_HEADER_SIZE) - current_length;

			auto append_length = std::min(need_length, static_cast<size_t>(length - consumed_length));
			_data.Append(data + consumed_length, append_length);
			consumed_length += append_length;

			if(_data.GetLength() >= MPEGTS_PES_HEADER_SIZE)
			{
				BitReader parser(_data.GetWritableDataAs<uint8_t>(), MPEGTS_PES_HEADER_SIZE);
				if(ParsePesHeader(&parser) == false)
				{
					logte("Could not parse table header");
					return 0;
				}
			}
		}

		if(_pes_optional_header_parsed == false && HasOptionalHeader())
		{
			// Parsing header first
			auto current_length = _data.GetLength();
			// needed data length for parsing pes optional header (9 - current length)
			// need_length cannot be minus value
			auto need_length = static_cast<size_t>(MPEGTS_PES_HEADER_SIZE + MPEGTS_MIN_PES_OPTIONAL_HEADER_SIZE) - current_length;

			auto append_length = std::min(need_length, static_cast<size_t>(length - consumed_length));
			_data.Append(data + consumed_length, append_length);
			consumed_length += append_length;

			if(_data.GetLength() >= MPEGTS_PES_HEADER_SIZE + MPEGTS_MIN_PES_OPTIONAL_HEADER_SIZE)
			{
				BitReader parser(_data.GetWritableDataAs<uint8_t>(), MPEGTS_PES_HEADER_SIZE + MPEGTS_MIN_PES_OPTIONAL_HEADER_SIZE);
				// PES header is already parsed so skips header
				parser.SkipBytes(MPEGTS_PES_HEADER_SIZE);
				if(ParsePesOptionalHeader(&parser) == false)
				{
					logte("Could not parse table header");
					return 0;
				}
			}
		}

		if(_pes_optional_data_parsed == false && HasOptionalData())
		{
			// Parsing header first
			auto current_length = _data.GetLength();
			// needed data length for parsing pes optional header (9 + _header_data_length - current length)
			// need_length cannot be minus value
			auto need_length = static_cast<size_t>(MPEGTS_PES_HEADER_SIZE + MPEGTS_MIN_PES_OPTIONAL_HEADER_SIZE + _header_data_length) - current_length;

			auto append_length = std::min(need_length, static_cast<size_t>(length - consumed_length));
			_data.Append(data + consumed_length, append_length);
			consumed_length += append_length;

			if(_data.GetLength() >= MPEGTS_PES_HEADER_SIZE + MPEGTS_MIN_PES_OPTIONAL_HEADER_SIZE + _header_data_length)
			{
				BitReader parser(_data.GetWritableDataAs<uint8_t>(), MPEGTS_PES_HEADER_SIZE + MPEGTS_MIN_PES_OPTIONAL_HEADER_SIZE + _header_data_length);
				// PES header and optional header are already parsed so skips that
				parser.SkipBytes(MPEGTS_PES_HEADER_SIZE + MPEGTS_MIN_PES_OPTIONAL_HEADER_SIZE);
				if(ParsePesOPtionalData(&parser) == false)
				{
					logte("Could not parse table header");
					return 0;
				}
			}
		}

		// all inputted data is consumed, wait for next packet
		if(consumed_length == length)
		{
			return length;
		}
		
		uint16_t remained_packet_length = 0;
		uint16_t copy_length = length - consumed_length;
		if(_pes_packet_length != 0)
		{
			// How many bytes remains to complete this pes packet
			remained_packet_length = _pes_packet_length - (_data.GetLength() - MPEGTS_PES_HEADER_SIZE);
			copy_length = std::min(copy_length, remained_packet_length);
		}
		
		// If all header and optional data are parsed, all remaining data is payload
		_data.Append(data + consumed_length, copy_length);
		consumed_length += copy_length;

		// Completed
		if(_pes_packet_length != 0 && _pes_packet_length == _data.GetLength() - MPEGTS_PES_HEADER_SIZE)
		{
			SetEndOfData();
		}

		return consumed_length;
	}

	bool Pes::ParsePesHeader(BitReader *parser)
	{
		_start_code_prefix_1 = parser->ReadBytes<uint8_t>();
		_start_code_prefix_2 = parser->ReadBytes<uint8_t>();
		_start_code_prefix_3 = parser->ReadBytes<uint8_t>();
		if(_start_code_prefix_1 != 0x00 || _start_code_prefix_2 != 0x00 || _start_code_prefix_3 != 0x01)
		{	
			logtw("Could not parse pes header. (pid: %d)", _pid);
			return false;
		}

		_stream_id = parser->ReadBytes<uint8_t>();
		_pes_packet_length = parser->ReadBytes<uint16_t>();

		_pes_header_parsed = true;
		return true;
	}

	bool Pes::ParsePesOptionalHeader(BitReader *parser)
	{
		_marker_bits = parser->ReadBits<uint8_t>(2);
		_scrambling_control = parser->ReadBits<uint8_t>(2);
		_priority = parser->ReadBit();
		_data_alignment_indicator = parser->ReadBit();
		_copyright = parser->ReadBit();
		_original_or_copy = parser->ReadBit();
		_pts_dts_flags = parser->ReadBits<uint8_t>(2);
		_escr_flag = parser->ReadBit();
		_es_rate_flag = parser->ReadBit();
		_dsm_trick_mode_flag = parser->ReadBit();
		_additional_copy_info_flag = parser->ReadBit();
		_crc_flag = parser->ReadBit();
		_extension_flag = parser->ReadBit();
		_header_data_length = parser->ReadBytes<uint8_t>();

		_pes_optional_header_parsed = true;
		return true;
	}

	bool Pes::ParsePesOPtionalData(BitReader *parser)
	{
		parser->StartSection();

		bool pts_flag = OV_GET_BIT(_pts_dts_flags, 1);
		bool dts_flag = OV_GET_BIT(_pts_dts_flags, 0);

		// only pts
		if(pts_flag == 1 && dts_flag == 0)
		{
			if(ParseTimestamp(parser, 0b0010, _pts) == false)
			{
				logte("Could not parse PTS");
				return false;
			}

			_dts = _pts;
		}
		// pts, dts
		else if(pts_flag == 1 && dts_flag == 1)
		{
			if ((ParseTimestamp(parser, 0b0011, _pts) == false) ||
				(ParseTimestamp(parser, 0b0001, _dts) == false))
			{
				logte("Could not parse PTS & DTS");
				return false;
			}
		}
		// 01: forbidden, 00: no pts, dts
		else
		{
			logte("Could not parse PTS & DTS");
			return false;
		}

		auto parsed_optional_fields_length = parser->BytesSetionConsumed();
		
		// Another options are not used now, so they are skipped
		parser->SkipBytes(_header_data_length - parsed_optional_fields_length);

		_pes_optional_data_parsed = true;
		return true;
	}

	bool Pes::ParseTimestamp(BitReader *parser, uint8_t start_bits, int64_t &timestamp)
	{
		//  76543210  76543210  76543210  76543210  76543210
		// [ssssTTTm][TTTTTTTT][TTTTTTTm][TTTTTTTT][TTTTTTTm]
		//
		//      TTT   TTTTTTTT  TTTTTTT   TTTTTTTT  TTTTTTT
		//      210   98765432  1098765   43210987  6543210
		//        3              2            1           0
		// s: start_bits
		// m: marker_bit (should be 1)
		// T: timestamp

		auto prefix = parser->ReadBits<uint8_t>(4);
		if (prefix != start_bits)
		{
			logte("Invalid PTS_DTS start bits: %d (%02X), expected: %d (%02X)", prefix, prefix, start_bits, start_bits);
			return false;
		}

		int64_t ts = parser->ReadBits<int64_t>(3) << 30;

		// Check the first marker bit of the first byte
		if (parser->ReadBit() != 0b1)
		{
			logte("Invalid marker bit of the first byte");
			return false;
		}

		ts |= parser->ReadBits<int64_t>(8) << 22;
		ts |= parser->ReadBits<int64_t>(7) << 15;

		// Check the second marker bit of the third byte
		if (parser->ReadBit() != 0b1)
		{
			logte("Invalid marker bit of the third byte");
			return false;
		}

		ts |= parser->ReadBits<int64_t>(8) << 7;
		ts |= parser->ReadBits<int64_t>(7);

		// Check the third marker bit of the fifth byte
		if (parser->ReadBit() != 0b1)
		{
			logte("Invalid marker bit of the third byte");
			return false;
		}

		timestamp = ts;

		return true;
	}


	// All pes data has been inserted
	bool Pes::SetEndOfData()
	{
		// Set payload
		_payload = _data.GetWritableDataAs<uint8_t>();
		_payload_length = _data.GetLength();

		_payload += MPEGTS_PES_HEADER_SIZE;
		_payload_length -= MPEGTS_PES_HEADER_SIZE;

		if(IsAudioStream() || IsVideoStream())
		{
			_payload += MPEGTS_MIN_PES_OPTIONAL_HEADER_SIZE + _header_data_length;
			_payload_length -= MPEGTS_MIN_PES_OPTIONAL_HEADER_SIZE + _header_data_length;
		}

		_completed = true;
		return true;
	}

	bool Pes::HasOptionalHeader()
	{
		return _pes_header_parsed && (IsAudioStream() || IsVideoStream());
	}

	bool Pes::HasOptionalData()
	{
		return HasOptionalHeader() && _pes_optional_header_parsed && (_header_data_length > 0);
	}

	// return true when section is completed
	bool Pes::IsCompleted()
	{
		return _completed;
	}

	// Getter
	uint16_t Pes::PID()
	{
		return _pid;
	}

	uint8_t Pes::StreamId()
	{
		return _stream_id;
	}

	uint16_t Pes::PesPacketLength()
	{
		return _pes_packet_length;
	}

	uint8_t Pes::ScramblingControl()
	{
		return _scrambling_control;
	}

	uint8_t Pes::Priority()
	{
		return _priority;
	}
	
	uint8_t Pes::DataAlignmentIndicator()
	{
		return _data_alignment_indicator;
	}
	uint8_t Pes::Copyright()
	{
		return _copyright;
	}
	
	uint8_t Pes::OriginalOrCopy()
	{
		return _original_or_copy;
	}
	
	int64_t Pes::Pts()
	{
		return _pts;
	}
	
	int64_t Pes::Dts()
	{
		return _dts;
	}
	
	const uint8_t* Pes::Payload()
	{
		return _payload;
	}
	
	uint32_t Pes::PayloadLength()
	{
		return _payload_length;
	}
}