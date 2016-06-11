FATE_AFILTER-$(call FILTERDEMDECENCMUX, ADELAY, WAV, PCM_S16LE, PCM_S16LE, WAV) += fate-filter-adelay
fate-filter-adelay: tests/data/asynth-44100-2.wav
fate-filter-adelay: SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-adelay: CMD = framecrc -i $(SRC) -af adelay=42

FATE_AFILTER-$(call FILTERDEMDECENCMUX, AECHO, WAV, PCM_S16LE, PCM_S16LE, WAV) += fate-filter-aecho
fate-filter-aecho: tests/data/asynth-44100-2.wav
fate-filter-aecho: SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-aecho: CMD = framecrc -i $(SRC) -af aecho=0.5:0.5:32:0.5

FATE_FILTER_AEMPHASIS += fate-filter-aemphasis-50fm
fate-filter-aemphasis-50fm: tests/data/asynth-44100-2.wav
fate-filter-aemphasis-50fm: SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-aemphasis-50fm: CMD = framecrc -i $(SRC) -af aemphasis=1:5:reproduction:50fm

FATE_FILTER_AEMPHASIS += fate-filter-aemphasis-75kf
fate-filter-aemphasis-75kf: tests/data/asynth-44100-2.wav
fate-filter-aemphasis-75kf: SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-aemphasis-75kf: CMD = framecrc -i $(SRC) -af aemphasis=2:8:reproduction:75kf

FATE_AFILTER-$(call FILTERDEMDECENCMUX, AEMPHASIS, WAV, PCM_S16LE, PCM_S16LE, WAV) += $(FATE_FILTER_AEMPHASIS)

FATE_FILTER_AFADE += fate-filter-afade-qsin
fate-filter-afade-qsin: tests/data/asynth-44100-2.wav
fate-filter-afade-qsin: SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-afade-qsin: CMD = framecrc -i $(SRC) -af afade=t=in:ss=0:d=2:curve=qsin

FATE_FILTER_AFADE += fate-filter-afade-iqsin
fate-filter-afade-iqsin: tests/data/asynth-44100-2.wav
fate-filter-afade-iqsin: SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-afade-iqsin: CMD = framecrc -i $(SRC) -af afade=t=in:ss=0:d=2:curve=iqsin

FATE_FILTER_AFADE += fate-filter-afade-esin
fate-filter-afade-esin: tests/data/asynth-44100-2.wav
fate-filter-afade-esin: SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-afade-esin: CMD = framecrc -i $(SRC) -af afade=t=in:ss=0:d=2:curve=esin

FATE_FILTER_AFADE += fate-filter-afade-hsin
fate-filter-afade-hsin: tests/data/asynth-44100-2.wav
fate-filter-afade-hsin: SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-afade-hsin: CMD = framecrc -i $(SRC) -af afade=t=in:ss=0:d=2:curve=hsin

FATE_FILTER_AFADE += fate-filter-afade-exp
fate-filter-afade-exp: tests/data/asynth-44100-2.wav
fate-filter-afade-exp: SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-afade-exp: CMD = framecrc -i $(SRC) -af afade=t=in:ss=0:d=2:curve=exp

FATE_FILTER_AFADE += fate-filter-afade-log
fate-filter-afade-log: tests/data/asynth-44100-2.wav
fate-filter-afade-log: SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-afade-log: CMD = framecrc -i $(SRC) -af afade=t=in:ss=1:d=2.5:curve=log

FATE_AFILTER-$(call FILTERDEMDECENCMUX, AFADE, WAV, PCM_S16LE, PCM_S16LE, WAV) += $(FATE_FILTER_AFADE)

FATE_AFILTER-$(call FILTERDEMDECENCMUX, ACROSSFADE, WAV, PCM_S16LE, PCM_S16LE, WAV) += fate-filter-acrossfade
fate-filter-acrossfade: tests/data/asynth-44100-2.wav
fate-filter-acrossfade: SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-acrossfade: SRC2 = $(TARGET_SAMPLES)/audio-reference/luckynight_2ch_44kHz_s16.wav
fate-filter-acrossfade: CMD = framecrc -i $(SRC) -i $(SRC2) -filter_complex acrossfade=d=2:c1=log:c2=exp

tests/data/hls-list.m3u8: TAG = GEN
tests/data/hls-list.m3u8: ffmpeg$(PROGSSUF)$(EXESUF) | tests/data
	$(M)$(TARGET_EXEC) $(TARGET_PATH)/$< \
        -f lavfi -i "aevalsrc=cos(2*PI*t)*sin(2*PI*(440+4*t)*t)::d=20" -f segment -segment_time 10 -map 0 -flags +bitexact -codec:a mp2fixed \
        -segment_list $(TARGET_PATH)/$@ -y $(TARGET_PATH)/tests/data/hls-out-%03d.ts 2>/dev/null

FATE_AFILTER-$(call ALLYES, HLS_DEMUXER MPEGTS_MUXER MPEGTS_DEMUXER AEVALSRC_FILTER LAVFI_INDEV MP2FIXED_ENCODER) += fate-filter-hls
fate-filter-hls: tests/data/hls-list.m3u8
fate-filter-hls: CMD = framecrc -flags +bitexact -i $(TARGET_PATH)/tests/data/hls-list.m3u8

FATE_AMIX += fate-filter-amix-simple
fate-filter-amix-simple: CMD = ffmpeg -filter_complex amix -i $(SRC) -ss 3 -i $(SRC1) -f f32le -
fate-filter-amix-simple: REF = $(SAMPLES)/filter/amix_simple.pcm

FATE_AMIX += fate-filter-amix-first
fate-filter-amix-first: CMD = ffmpeg -filter_complex amix=duration=first -ss 4 -i $(SRC) -i $(SRC1) -f f32le -
fate-filter-amix-first: REF = $(SAMPLES)/filter/amix_first.pcm

FATE_AMIX += fate-filter-amix-transition
fate-filter-amix-transition: tests/data/asynth-44100-2-3.wav
fate-filter-amix-transition: SRC2 = $(TARGET_PATH)/tests/data/asynth-44100-2-3.wav
fate-filter-amix-transition: CMD = ffmpeg -filter_complex amix=inputs=3:dropout_transition=0.5 -i $(SRC) -ss 2 -i $(SRC1) -ss 4 -i $(SRC2) -f f32le -
fate-filter-amix-transition: REF = $(SAMPLES)/filter/amix_transition.pcm

FATE_AFILTER_SAMPLES-$(call FILTERDEMDECENCMUX, AMIX, WAV, PCM_S16LE, PCM_F32LE, PCM_F32LE) += $(FATE_AMIX)
$(FATE_AMIX): tests/data/asynth-44100-2.wav tests/data/asynth-44100-2-2.wav
$(FATE_AMIX): SRC  = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
$(FATE_AMIX): SRC1 = $(TARGET_PATH)/tests/data/asynth-44100-2-2.wav
$(FATE_AMIX): CMP  = oneoff
$(FATE_AMIX): CMP_UNIT = f32

FATE_AFILTER_SAMPLES-$(call FILTERDEMDECMUX, ASYNCTS, FLV, NELLYMOSER, PCM_S16LE) += fate-filter-asyncts
fate-filter-asyncts: SRC = $(TARGET_SAMPLES)/nellymoser/nellymoser-discont.flv
fate-filter-asyncts: CMD = pcm -analyzeduration 10000000 -i $(SRC) -af asyncts
fate-filter-asyncts: CMP = oneoff
fate-filter-asyncts: REF = $(SAMPLES)/nellymoser/nellymoser-discont-async-v3.pcm

FATE_AFILTER_SAMPLES-$(CONFIG_ARESAMPLE_FILTER) += fate-filter-aresample
fate-filter-aresample: SRC = $(TARGET_SAMPLES)/nellymoser/nellymoser-discont.flv
fate-filter-aresample: CMD = pcm -analyzeduration 10000000 -i $(SRC) -af aresample=min_comp=0.001:min_hard_comp=0.1:first_pts=0
fate-filter-aresample: CMP = oneoff
fate-filter-aresample: REF = $(SAMPLES)/nellymoser/nellymoser-discont.pcm

FATE_ATRIM += fate-filter-atrim-duration
fate-filter-atrim-duration: CMD = framecrc -i $(SRC) -af atrim=start=0.1:duration=0.01
FATE_ATRIM += fate-filter-atrim-mixed
fate-filter-atrim-mixed: CMD = framecrc -i $(SRC) -af atrim=start=0.05:start_sample=1025:end=0.1:end_sample=4411

FATE_ATRIM += fate-filter-atrim-samples
fate-filter-atrim-samples: CMD = framecrc -i $(SRC) -af atrim=start_sample=26:end_sample=80

FATE_ATRIM += fate-filter-atrim-time
fate-filter-atrim-time: CMD = framecrc -i $(SRC) -af atrim=0.1:0.2

$(FATE_ATRIM): tests/data/asynth-44100-2.wav
$(FATE_ATRIM): SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav

FATE_AFILTER-$(call FILTERDEMDECENCMUX, ATRIM, WAV, PCM_S16LE, PCM_S16LE, WAV) += $(FATE_ATRIM)

FATE_FILTER_CHANNELMAP += fate-filter-channelmap-one-int
fate-filter-channelmap-one-int: tests/data/filtergraphs/channelmap_one_int
fate-filter-channelmap-one-int: SRC = $(TARGET_PATH)/tests/data/asynth-44100-6.wav
fate-filter-channelmap-one-int: tests/data/asynth-44100-6.wav
fate-filter-channelmap-one-int: CMD = md5 -i $(SRC) -filter_complex_script $(TARGET_PATH)/tests/data/filtergraphs/channelmap_one_int -f wav -fflags +bitexact
fate-filter-channelmap-one-int: CMP = oneline
fate-filter-channelmap-one-int: REF = 428b8f9fac6d57147069b97335019ef5

FATE_FILTER_CHANNELMAP += fate-filter-channelmap-one-str
fate-filter-channelmap-one-str: tests/data/filtergraphs/channelmap_one_str
fate-filter-channelmap-one-str: SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-channelmap-one-str: tests/data/asynth-44100-2.wav
fate-filter-channelmap-one-str: CMD = md5 -i $(SRC) -filter_complex_script $(TARGET_PATH)/tests/data/filtergraphs/channelmap_one_str -f wav -fflags +bitexact
fate-filter-channelmap-one-str: CMP = oneline
fate-filter-channelmap-one-str: REF = e788890db6a11c2fb29d7c4229072d49

FATE_AFILTER-$(call FILTERDEMDECENCMUX, CHANNELMAP, WAV, PCM_S16LE, PCM_S16LE, WAV) += $(FATE_FILTER_CHANNELMAP)

FATE_AFILTER-$(call FILTERDEMDECENCMUX, CHANNELSPLIT, WAV, PCM_S16LE, PCM_S16LE, PCM_S16LE) += fate-filter-channelsplit
fate-filter-channelsplit: SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-channelsplit: tests/data/asynth-44100-2.wav
fate-filter-channelsplit: CMD = md5 -i $(SRC) -filter_complex channelsplit -f s16le
fate-filter-channelsplit: CMP = oneline
fate-filter-channelsplit: REF = d92988d0fe2dd92236763f47b07ab597

FATE_AFILTER-$(call FILTERDEMDECENCMUX, JOIN, WAV, PCM_S16LE, PCM_S16LE, PCM_S16LE) += fate-filter-join
fate-filter-join: SRC1 = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-join: SRC2 = $(TARGET_PATH)/tests/data/asynth-44100-3.wav
fate-filter-join: tests/data/asynth-44100-2.wav tests/data/asynth-44100-3.wav
fate-filter-join: CMD = md5 -i $(SRC1) -i $(SRC2) -filter_complex join=channel_layout=5c -f s16le
fate-filter-join: CMP = oneline
fate-filter-join: REF = 88b0d24a64717ba8635b29e8dac6ecd8

FATE_AFILTER-$(call ALLYES, WAV_DEMUXER PCM_S16LE_DECODER PCM_S16LE_ENCODER PCM_S16LE_MUXER APERMS_FILTER VOLUME_FILTER) += fate-filter-volume
fate-filter-volume: SRC = $(TARGET_PATH)/tests/data/asynth-44100-2.wav
fate-filter-volume: tests/data/asynth-44100-2.wav
fate-filter-volume: CMD = md5 -i $(SRC) -af aperms=random,volume=precision=fixed:volume=0.5 -f s16le
fate-filter-volume: CMP = oneline
fate-filter-volume: REF = 4d6ba75ef3e32d305d066b9bc771d6f4

FATE_AFILTER_SAMPLES-$(call FILTERDEMDECENCMUX, HDCD, FLAC, FLAC, PCM_S24LE, PCM_S24LE) += fate-filter-hdcd
fate-filter-hdcd: SRC = $(TARGET_SAMPLES)/filter/hdcd.flac
fate-filter-hdcd: CMD = md5 -i $(SRC) -af hdcd -f s24le
fate-filter-hdcd: CMP = oneline
fate-filter-hdcd: REF = 5db465a58d2fd0d06ca944b883b33476

FATE_AFILTER-yes += fate-filter-formats
fate-filter-formats: libavfilter/formats-test$(EXESUF)
fate-filter-formats: CMD = run libavfilter/formats-test

FATE_SAMPLES_AVCONV += $(FATE_AFILTER_SAMPLES-yes)
FATE_FFMPEG += $(FATE_AFILTER-yes)
fate-afilter: $(FATE_AFILTER-yes) $(FATE_AFILTER_SAMPLES-yes)
