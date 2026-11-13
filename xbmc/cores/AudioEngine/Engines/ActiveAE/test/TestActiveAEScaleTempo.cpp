/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "cores/AudioEngine/Engines/ActiveAE/ActiveAEScaleTempo.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

extern "C"
{
#include <libavutil/channel_layout.h>
}

using namespace ActiveAE;

namespace
{
constexpr int SAMPLE_RATE = 48000;
constexpr int CHANNELS = 2;
constexpr int CHUNK = 1024;
constexpr int MAX_OUTPUT = 16384;

/*!
 * \brief Runs a 440 Hz tone through the filter, keeping the left channel of the output
 */
class CToneFeeder
{
public:
  CToneFeeder(CActiveAEScaleTempo& filter, bool planar) : m_filter(filter), m_planar(planar)
  {
    m_filter.Init(planar ? AV_SAMPLE_FMT_FLTP : AV_SAMPLE_FMT_FLT, SAMPLE_RATE,
                  AV_CH_LAYOUT_STEREO);
  }

  void Feed(int samples)
  {
    std::vector<std::vector<float>> src(CHANNELS, std::vector<float>(CHUNK * CHANNELS));
    uint8_t* srcPlanes[CHANNELS];
    for (int ch = 0; ch < CHANNELS; ++ch)
      srcPlanes[ch] = reinterpret_cast<uint8_t*>(src[ch].data());

    for (int fed = 0; fed < samples; fed += CHUNK)
    {
      for (int i = 0; i < CHUNK; ++i)
      {
        const auto value = static_cast<float>(
            std::sin(2.0 * std::numbers::pi * 440.0 * (m_fed + i) / SAMPLE_RATE));
        for (int ch = 0; ch < CHANNELS; ++ch)
        {
          if (m_planar)
            src[ch][i] = value;
          else
            src[0][i * CHANNELS + ch] = value;
        }
      }
      m_fed += CHUNK;

      Take(m_filter.ProcessFilter(DstPlanes(), MAX_OUTPUT, srcPlanes, CHUNK,
                                  CHUNK * CHANNELS * static_cast<int>(sizeof(float))));
      while (!m_filter.NeedData() && !m_filter.IsEof())
        Take(m_filter.ProcessFilter(DstPlanes(), MAX_OUTPUT, nullptr, 0, 0));
    }
  }

  void Drain()
  {
    for (int i = 0; i < 1000 && !m_filter.IsEof(); ++i)
      Take(m_filter.ProcessFilter(DstPlanes(), MAX_OUTPUT, nullptr, 0, 0));
  }

  int Fed() const { return m_fed; }
  const std::vector<float>& Output() const { return m_output; }

private:
  uint8_t** DstPlanes()
  {
    for (int ch = 0; ch < CHANNELS; ++ch)
    {
      m_dst[ch].resize(MAX_OUTPUT * CHANNELS);
      m_dstPlanes[ch] = reinterpret_cast<uint8_t*>(m_dst[ch].data());
    }
    return m_dstPlanes;
  }

  void Take(int samples)
  {
    ASSERT_GE(samples, 0);
    for (int i = 0; i < samples; ++i)
      m_output.push_back(m_planar ? m_dst[0][i] : m_dst[0][i * CHANNELS]);
  }

  CActiveAEScaleTempo& m_filter;
  bool m_planar;
  int m_fed = 0;
  std::vector<float> m_dst[CHANNELS];
  uint8_t* m_dstPlanes[CHANNELS];
  std::vector<float> m_output;
};

class TestActiveAEScaleTempo : public ::testing::TestWithParam<std::tuple<float, bool>>
{
};
} // namespace

TEST_P(TestActiveAEScaleTempo, OutputLengthFollowsTempo)
{
  const auto [tempo, planar] = GetParam();

  CActiveAEScaleTempo filter;
  CToneFeeder feeder(filter, planar);
  ASSERT_TRUE(filter.SetTempo(tempo));
  ASSERT_TRUE(filter.IsActive());

  feeder.Feed(2 * SAMPLE_RATE);
  feeder.Drain();
  ASSERT_TRUE(filter.IsEof());

  // All of the input is put out at the tempo, without the silence the end is padded with. Slowed
  // down, the end can come up to a window (12 ms of input) early, as in mpv.
  const double expected = feeder.Fed() / static_cast<double>(tempo);
  const double tolerance = tempo < 1.0f ? 0.012 * SAMPLE_RATE / static_cast<double>(tempo) : 2.0;
  EXPECT_NEAR(static_cast<double>(feeder.Output().size()), expected, tolerance);
}

TEST_P(TestActiveAEScaleTempo, KeepsTheTone)
{
  const auto [tempo, planar] = GetParam();

  CActiveAEScaleTempo filter;
  CToneFeeder feeder(filter, planar);
  ASSERT_TRUE(filter.SetTempo(tempo));
  feeder.Feed(2 * SAMPLE_RATE);

  // The pitch stays, so the tone keeps its period: count rising zero crossings over the output
  const std::vector<float>& out = feeder.Output();
  ASSERT_GT(out.size(), static_cast<size_t>(SAMPLE_RATE / 4));
  int crossings = 0;
  for (size_t i = 1; i < out.size(); ++i)
  {
    if (out[i - 1] < 0.0f && out[i] >= 0.0f)
      ++crossings;
  }
  const double frequency = crossings * static_cast<double>(SAMPLE_RATE) / out.size();
  EXPECT_NEAR(frequency, 440.0, 440.0 * 0.03);
}

INSTANTIATE_TEST_SUITE_P(,
                         TestActiveAEScaleTempo,
                         ::testing::Combine(::testing::Values(0.5f, 1.5f, 2.085f, 4.17f),
                                            ::testing::Bool()));

TEST(TestActiveAEScaleTempoState, InactiveAtOne)
{
  CActiveAEScaleTempo filter;
  filter.Init(AV_SAMPLE_FMT_FLTP, SAMPLE_RATE, AV_CH_LAYOUT_STEREO);
  EXPECT_FALSE(filter.IsActive());

  EXPECT_FALSE(filter.ChangeTempo(2.0f));
  EXPECT_FALSE(filter.IsActive());

  ASSERT_TRUE(filter.SetTempo(2.0f));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_FALSE(filter.ChangeTempo(1.0f));
  EXPECT_TRUE(filter.IsActive());

  ASSERT_TRUE(filter.SetTempo(1.0f));
  EXPECT_FALSE(filter.IsActive());
}

TEST(TestActiveAEScaleTempoState, OnlyFloatIsSupported)
{
  CActiveAEScaleTempo filter;
  filter.Init(AV_SAMPLE_FMT_S16, SAMPLE_RATE, AV_CH_LAYOUT_STEREO);
  EXPECT_FALSE(filter.SetTempo(2.0f));
  EXPECT_FALSE(filter.IsActive());
}

TEST(TestActiveAEScaleTempoState, ChangeTempoKeepsBufferedAudio)
{
  CActiveAEScaleTempo filter;
  CToneFeeder feeder(filter, true);
  ASSERT_TRUE(filter.SetTempo(1.5f));
  feeder.Feed(8 * CHUNK);

  const int buffered = filter.GetBufferedSamples();
  ASSERT_GT(buffered, 0);

  ASSERT_TRUE(filter.ChangeTempo(2.0f));
  EXPECT_TRUE(filter.IsActive());
  EXPECT_FALSE(filter.IsEof());
  // Held output counts as input at the tempo it is held at
  EXPECT_NEAR(filter.GetBufferedSamples(), buffered, CHUNK);

  // Starting over drops it
  ASSERT_TRUE(filter.SetTempo(2.0f));
  EXPECT_EQ(filter.GetBufferedSamples(), 0);
  EXPECT_TRUE(filter.NeedData());
}

TEST(TestActiveAEScaleTempoState, DrainWithoutInputEnds)
{
  CActiveAEScaleTempo filter;
  filter.Init(AV_SAMPLE_FMT_FLTP, SAMPLE_RATE, AV_CH_LAYOUT_STEREO);
  ASSERT_TRUE(filter.SetTempo(2.0f));
  ASSERT_TRUE(filter.NeedData());

  float left[CHUNK];
  float right[CHUNK];
  uint8_t* dst[] = {reinterpret_cast<uint8_t*>(left), reinterpret_cast<uint8_t*>(right)};
  EXPECT_EQ(filter.ProcessFilter(dst, CHUNK, nullptr, 0, 0), 0);
  EXPECT_TRUE(filter.IsEof());
  EXPECT_FALSE(filter.IsActive());

  // Input after the end is an error
  uint8_t* src[] = {reinterpret_cast<uint8_t*>(left), reinterpret_cast<uint8_t*>(right)};
  EXPECT_EQ(filter.ProcessFilter(dst, CHUNK, src, CHUNK, CHUNK * 2 * sizeof(float)), -1);
}

TEST(TestActiveAEScaleTempoState, TempoRampHasNoGaps)
{
  // Tempo follows the clock, changing it in place must not mute audio within the range of the pool
  CActiveAEScaleTempo filter;
  CToneFeeder feeder(filter, true);
  ASSERT_TRUE(filter.SetTempo(0.5f));

  for (float tempo = 0.5f; tempo <= 8.0f; tempo += 0.25f)
  {
    // Changing to 1 stops the filter
    if (tempo == 1.0f)
      continue;
    ASSERT_TRUE(filter.ChangeTempo(tempo));
    feeder.Feed(4 * CHUNK);
  }

  // A 440 Hz tone never stays near zero for more than a fraction of its period
  const std::vector<float>& out = feeder.Output();
  ASSERT_GT(out.size(), static_cast<size_t>(SAMPLE_RATE / 4));
  int quiet = 0;
  int longestQuiet = 0;
  for (float sample : out)
  {
    quiet = std::fabs(sample) < 0.05f ? quiet + 1 : 0;
    longestQuiet = std::max(longestQuiet, quiet);
  }
  EXPECT_LT(longestQuiet, SAMPLE_RATE / 440 / 4);
}
