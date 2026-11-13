/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ActiveAEScaleTempo.h"

#include "utils/log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

extern "C"
{
#include <libavutil/channel_layout.h>
}

using namespace ActiveAE;

CActiveAEScaleTempo::CActiveAEScaleTempo()
{
  // mpv's defaults, audio is muted outside of the playback rates
  m_opts.min_playback_rate = 0.25f;
  m_opts.max_playback_rate = 8.0f;
  m_opts.ola_window_size_ms = 12.0f;
  m_opts.wsola_search_interval_ms = 40.0f;
  m_data.opts = &m_opts;
}

CActiveAEScaleTempo::~CActiveAEScaleTempo()
{
  mp_scaletempo2_destroy(&m_data);
}

void CActiveAEScaleTempo::Init(AVSampleFormat fmt, int sampleRate, uint64_t channelLayout)
{
  AVChannelLayout layout = {};
  av_channel_layout_from_mask(&layout, channelLayout);
  m_channels = layout.nb_channels;
  av_channel_layout_uninit(&layout);

  m_planar = fmt == AV_SAMPLE_FMT_FLTP;
  m_initialized = (fmt == AV_SAMPLE_FMT_FLTP || fmt == AV_SAMPLE_FMT_FLT) && m_channels > 0;
  if (!m_initialized)
  {
    CLog::Log(LOGERROR, "CActiveAEScaleTempo::Init - unsupported format {} with {} channels",
              av_get_sample_fmt_name(fmt) ? av_get_sample_fmt_name(fmt) : "none", m_channels);
    return;
  }

  mp_scaletempo2_init(&m_data, m_channels, sampleRate);
  m_pending.assign(m_channels, {});
  m_pendingPlanes.assign(m_channels, nullptr);
  m_output.assign(m_planar ? 0 : m_channels, {});
  m_outputPlanes.assign(m_planar ? 0 : m_channels, nullptr);
  m_tempo = 1.0f;
  Reset();
}

void CActiveAEScaleTempo::Reset()
{
  if (m_initialized)
    mp_scaletempo2_reset(&m_data);
  m_final = false;
  m_eof = false;
  m_needData = true;
  m_finalSamples = 0.0;
  m_pendingOffset = 0;
  m_pendingSamples = 0;
}

bool CActiveAEScaleTempo::SetTempo(float tempo)
{
  m_tempo = tempo;
  Reset();
  return m_initialized || tempo == 1.0f;
}

bool CActiveAEScaleTempo::ChangeTempo(float tempo)
{
  if (!IsActive() || tempo == 1.0f)
    return false;

  m_tempo = tempo;
  return true;
}

void CActiveAEScaleTempo::FeedPending()
{
  while (m_pendingSamples > 0)
  {
    for (int ch = 0; ch < m_channels; ++ch)
      m_pendingPlanes[ch] = reinterpret_cast<uint8_t*>(m_pending[ch].data() + m_pendingOffset);

    const int read = mp_scaletempo2_fill_input_buffer(
        &m_data, m_pendingPlanes.data(), m_pendingSamples, static_cast<double>(m_tempo));
    if (read <= 0)
      break;

    m_pendingOffset += read;
    m_pendingSamples -= read;
  }
}

int CActiveAEScaleTempo::ProcessFilter(
    uint8_t** dst_buffer, int dst_samples, uint8_t** src_buffer, int src_samples, int src_bufsize)
{
  if (!IsActive())
  {
    if (src_samples)
    {
      CLog::Log(LOGERROR, "CActiveAEScaleTempo::ProcessFilter - adding data while not active");
      return -1;
    }
    return 0;
  }

  const auto rate = static_cast<double>(m_tempo);

  if (src_samples)
  {
    if (m_pendingSamples || m_final)
    {
      CLog::Log(LOGERROR, "CActiveAEScaleTempo::ProcessFilter - adding data while not needed");
      return -1;
    }

    const auto bytes = static_cast<size_t>(src_samples) * m_channels * sizeof(float);
    if (src_bufsize < 0 || static_cast<size_t>(src_bufsize) < bytes)
    {
      CLog::Log(LOGERROR, "CActiveAEScaleTempo::ProcessFilter - input buffer too small");
      return -1;
    }

    for (int ch = 0; ch < m_channels; ++ch)
    {
      std::vector<float>& plane = m_pending[ch];
      plane.resize(src_samples);
      if (m_planar)
      {
        std::memcpy(plane.data(), src_buffer[ch], src_samples * sizeof(float));
      }
      else
      {
        const auto* src = reinterpret_cast<const float*>(src_buffer[0]);
        for (int i = 0; i < src_samples; ++i)
          plane[i] = src[i * m_channels + ch];
      }
    }
    m_pendingOffset = 0;
    m_pendingSamples = src_samples;
  }
  else if (!m_final && m_needData)
  {
    // End of the stream, the rest is drained
    mp_scaletempo2_set_final(&m_data);
    m_final = true;
    m_finalSamples = mp_scaletempo2_get_latency(&m_data, rate);
  }

  FeedPending();

  if (m_final)
  {
    // As mpv, stop where the input ends instead of putting out the silence it's padded with
    const int left = std::max(0, static_cast<int>(m_finalSamples / rate));
    if (left == 0 || !mp_scaletempo2_frames_available(&m_data, rate))
    {
      m_eof = true;
      return 0;
    }
    dst_samples = std::min(dst_samples, left);
  }

  float** dst = reinterpret_cast<float**>(dst_buffer);
  if (!m_planar)
  {
    for (int ch = 0; ch < m_channels; ++ch)
    {
      m_output[ch].resize(std::max(dst_samples, 0));
      m_outputPlanes[ch] = m_output[ch].data();
    }
    dst = m_outputPlanes.data();
  }

  const int out = mp_scaletempo2_fill_buffer(&m_data, dst, dst_samples, rate);

  if (!m_planar)
  {
    auto* interleaved = reinterpret_cast<float*>(dst_buffer[0]);
    for (int i = 0; i < out; ++i)
    {
      for (int ch = 0; ch < m_channels; ++ch)
        interleaved[i * m_channels + ch] = m_output[ch][i];
    }
  }

  const bool available = mp_scaletempo2_frames_available(&m_data, rate);
  m_needData = m_pendingSamples == 0 && (out == 0 || !available);

  if (m_final)
  {
    m_finalSamples -= out * rate;
    if (static_cast<int>(m_finalSamples / rate) <= 0 || !available)
      m_eof = true;
  }

  return out;
}

bool CActiveAEScaleTempo::NeedData() const
{
  return m_needData;
}

bool CActiveAEScaleTempo::IsEof() const
{
  return m_eof;
}

bool CActiveAEScaleTempo::IsActive() const
{
  return m_initialized && m_tempo != 1.0f && !m_eof;
}

int CActiveAEScaleTempo::GetBufferedSamples() const
{
  if (!IsActive())
    return 0;

  const double latency = mp_scaletempo2_get_latency(const_cast<mp_scaletempo2*>(&m_data),
                                                    static_cast<double>(m_tempo));
  return m_pendingSamples + std::max(0, static_cast<int>(std::lround(latency)));
}
