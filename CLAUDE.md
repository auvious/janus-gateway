# Janus Gateway - Auvious Fork

## Overview

This is a fork of [meetecho/janus-gateway](https://github.com/meetecho/janus-gateway), a general-purpose WebRTC server. The fork is based on the upstream `v0.10.x` era (merge base commit `46ef1167`) with 46 auvious-specific commits on top.

## Git Remotes

- `origin` - https://github.com/auvious/janus-gateway.git (auvious fork)
- `upstream` - https://github.com/meetecho/janus-gateway (original meetecho repo)
- `github-private` - private mirror

Main branch: `auvious/master`

## Build System

- Autotools only (no meson). Run `./configure && make`.
- Feature guards for libnice use `AC_CHECK_LIB` with symbol checks (e.g., `HAVE_LIBNICE_TCP`, `HAVE_PORTRANGE`, `HAVE_LIBNICE_KEEPALIVE_CONNCHECK`).

## Auvious Custom Plugins

There are three custom plugins, each a modified fork of an upstream Janus plugin:

### 1. janus_auviousroom (based on janus_videoroom)

**Key modification**: Added `listener_creates_offer` parameter on subscriber join. When `true`, the browser/client always creates the SDP offer (instead of Janus creating it for subscribers). This was the primary motivation for forking the videoroom plugin.

**How it works**: When `listener_creates_offer=true`:
- Plugin does NOT send a JSEP offer with the `attached` event
- Client sends its own SDP offer later
- Plugin generates an SDP answer negotiating against the publisher's codecs
- Subscriber-side payload types and extmap IDs are tracked separately

**Features removed vs upstream**: AV1/H.265 codecs, VP9/H264 profiles, opus FEC, transport-wide CC, E2EE, string IDs, reference counting (uses older watchdog pattern), multistream/unified-plan, renegotiation/ICE restart, admin API, bitrate_cap, lock_record, close_pc.

### 2. janus_auviousrecordplay (based on janus_recordplay)

**Key modifications**:
- Removed `.nfo` file persistence entirely - recordings are ephemeral/in-memory only
- Added `record-generate-offer` and `record-process-answer` requests for reversed SDP flow (plugin generates offer, client sends answer)
- Recordings are removed from memory on hangup

**Features removed vs upstream**: Simulcast support, codec override API, E2EE, admin API, `.jcfg` config format.

### 3. janus_auvioussip (based on janus_sip)

**Key modifications**:
- **K8s deployment workarounds**: Environment variables `SDP_IP`, `CONTACT_IP`, `USE_OPTIONS_AND_RPORT_FOR_CONTACT`, `SESSION_TIMER`, `SIP_STACK_NO_LISTEN`
- **OPTIONS warmup**: Sends OPTIONS request before INVITE to discover NAT/port mapping via Via header for proper Contact header construction
- **SIP signalling dump**: `sip_dump_filename` parameter to dump all SIP signalling to a file
- **Payload type remapping**: Rewrites RTP payload types between browser and SIP sides

**Features removed vs upstream**: SIP transfers (REFER), SUBSCRIBE/NOTIFY, multiple simultaneous calls (helper/master sessions), incoming header prefix filtering, per-stream SRTP, separate audio/video remote IPs, DSCP/TOS, re-INVITE state machine.

## Core Modifications (non-plugin)

- `ice.c`: `keepalive-conncheck` support for libnice ICE agent (guarded by `HAVE_LIBNICE_KEEPALIVE_CONNCHECK`)
- `ice.c`: RTCP Sender Report preparation considering last RTP timestamp sent
- Various SIP-related fixes merged from upstream point releases (v0.7.2, v0.7.6, v0.10.2)

## Upstream Divergence (Migration Notes)

The fork point is commit `46ef1167`. Since then, upstream has:

- **Moved all sources to `src/` folder** (commit `e277486a`) - any merge will require path resolution
- **Multistream PeerConnections** (commit `4110eea4`) - massive rewrite of videoroom (7.7k -> 14.3k lines), fundamentally changes subscriber model to support unified-plan with multiple streams per PeerConnection
- **Reference counting** throughout all plugins replacing the older watchdog pattern
- **String ID support** for rooms and participants
- **New codecs**: AV1, H.265
- **E2EE** (Insertable Streams) support
- **SIP transfers, subscriptions, helpers** in the SIP plugin
- **52 commits to ice.c**, **42 to janus.c**, **32 to SDP handling**
- Latest upstream version: **v1.4.0**

### Migration Feasibility (verified 2026-04-15)

Upstream does NOT support subscriber-created SDP offers in any plugin. All auvious custom features must be re-ported:

- **auviousroom**: Upstream videoroom hard-blocks subscriber offers (`"Only publishers send offers"`). The `listener_creates_offer` feature must be reimplemented against the new multistream/unified-plan architecture.
- **auviousrecordplay**: Upstream recordplay requires client offer for recording. The `record-generate-offer`/`record-process-answer` reversed flow has no upstream equivalent.
- **auvioussip**: K8s workarounds (env var overrides, OPTIONS warmup, PT remapping, SIP dump) are deployment-specific and must be re-applied as patches.

Recommended approach: re-fork from upstream v1.4.0 and port auvious features as targeted patches rather than attempting a merge.
