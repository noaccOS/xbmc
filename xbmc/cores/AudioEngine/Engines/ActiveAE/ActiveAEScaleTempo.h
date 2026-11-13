/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "cores/AudioEngine/Engines/ActiveAE/ActiveAEScaleTempo2Internals.h"

#include <cstdint>
#include <vector>

extern "C"
{
#include <libavutil/samplefmt.h>
}

namespace ActiveAE
{

/*!
 * \brief Changes the tempo of float audio without changing its pitch, with mpv's scaletempo2.
 */
class CActiveAEScaleTempo
{
public:
  CActiveAEScaleTempo();
  ~CActiveAEScaleTempo();
  CActiveAEScaleTempo(const CActiveAEScaleTempo&) = delete;
  CActiveAEScaleTempo& operator=(const CActiveAEScaleTempo&) = delete;

  void Init(AVSampleFormat fmt, int sampleRate, uint64_t channelLayout);
  /*!
   * \brief Take input and put out audio at the tempo.
   *
   * Input is taken in full, without input while NeedData() the end of the stream is drained.
   *
   * \return the samples written to dst_buffer, -1 on error
   */
  int ProcessFilter(uint8_t** dst_buffer,
                    int dst_samples,
                    uint8_t** src_buffer,
                    int src_samples,
                    int src_bufsize);
  /*!
   * \brief Start over at a tempo, dropping what is buffered.
   *
   * \param tempo the tempo, 1.0 makes the filter inactive
   * \return true if the format is supported
   */
  bool SetTempo(float tempo);
  /*!
   * \brief Change the tempo of the running filter, keeping what it has buffered.
   *
   * \param tempo the new tempo
   * \return false if the tempo can't be changed in place and the filter has to be re-created
   *         with SetTempo, true otherwise
   */
  bool ChangeTempo(float tempo);
  bool NeedData() const;
  bool IsEof() const;
  bool IsActive() const;
  /*!
   * \brief The input samples taken that are not put out yet.
   */
  int GetBufferedSamples() const;

private:
  void Reset();
  void FeedPending();

  mp_scaletempo2_opts m_opts;
  mp_scaletempo2 m_data{};
  bool m_initialized{false};
  bool m_planar{true};
  int m_channels{0};
  float m_tempo{1.0f};
  bool m_needData{true};
  bool m_final{false};
  bool m_eof{false};
  // Input samples left to put out after the end of the stream
  double m_finalSamples{0.0};
  // Input the filter didn't take yet, one plane per channel
  std::vector<std::vector<float>> m_pending;
  std::vector<uint8_t*> m_pendingPlanes;
  int m_pendingOffset{0};
  int m_pendingSamples{0};
  // Planes for interleaved output
  std::vector<std::vector<float>> m_output;
  std::vector<float*> m_outputPlanes;
};

} // namespace ActiveAE
