/*! \file   janus_auviousrecordplay.c
 * \author Lorenzo Miniero <lorenzo@meetecho.com>
 * \copyright GNU General Public License v3
 * \brief  Janus Record&Play plugin
 * \details  This is a simple application that implements two different
 * features: it allows you to record a message you send with WebRTC in
 * the format defined in recorded.c (MJR recording) and subsequently
 * replay this recording (or other previously recorded) through WebRTC
 * as well.
 *
 * This application aims at showing how easy recording frames sent by
 * a peer is, and how this recording can be re-used directly, without
 * necessarily involving a post-processing process (e.g., through the
 * tool we provide in janus-pp-rec.c). Notice that only audio and video
 * can be recorded and replayed in this plugin: if you're interested in
 * recording data channel messages (which Janus and the .mjr format do
 * support), you should use a different plugin instead.
 *
 * The configuration process is quite easy: just choose where the
 * recordings should be saved. The same folder will also be used to list
 * the available recordings that can be replayed.
 *
 * \note This application does not create a special file in INI format with
 * \c .nfo extension for each recording that is saved anymore. This is not necessary
 * as we use rtc-recorder specific metadata to map a specific audio .mjr file 
 * to its corresponding video .mjr one, since they always get saved in different files. 
 * \section recplayapi Record&Play API
 *
 * The Record&Play API supports several requests, some of which are
 * synchronous and some asynchronous. There are some situations, though,
 * (invalid JSON, invalid request) which will always result in a
 * synchronous error response even for asynchronous requests.
 *
 * \c list and \c update are synchronous requests, which means you'll
 * get a response directly within the context of the transaction. \c list
 * lists all the available recordings, while \c update forces the plugin
 * to scan the folder of recordings again in case some were added manually
 * and not indexed in the meanwhile.
 *
 * The \c record , \c play , \c start and \c stop requests instead are
 * all asynchronous, which means you'll get a notification about their
 * success or failure in an event. \c record asks the plugin to start
 * recording a session; \c play asks the plugin to prepare the playout
 * of one of the previously recorded sessions; \c start starts the
 * actual playout, and \c stop stops whatever the session was for, i.e.,
 * recording or replaying.
 *
 * The \c list request has to be formatted as follows:
 *
\verbatim
{
	"request" : "list"
}
\endverbatim
 *
 * A successful request will result in an array of recordings:
 *
\verbatim
{
	"recordplay" : "list",
	"list": [	// Array of recording objects
		{			// Recording #1
			"id": <numeric ID>,
			"name": "<Name of the recording>",
			"date": "<Date of the recording>",
			"audio": "<Audio rec file, if any; optional>",
			"video": "<Video rec file, if any; optional>",
			"audio_codec": "<Audio codec, if any; optional>",
			"video_codec": "<Video codec, if any; optional>"
		},
		<other recordings>
	]
}
\endverbatim
 *
 * An error instead (and the same applies to all other requests, so this
 * won't be repeated) would provide both an error code and a more verbose
 * description of the cause of the issue:
 *
\verbatim
{
	"recordplay" : "event",
	"error_code" : <numeric ID, check Macros below>,
	"error" : "<error description as a string>"
}
\endverbatim
 *
 * The \c update request instead has to be formatted as follows:
 *
\verbatim
{
	"request" : "update"
}
\endverbatim
 *
 * which will always result in an immediate ack ( \c ok ):
 *
\verbatim
{
	"recordplay" : "ok",
}
\endverbatim
 *
 * Coming to the asynchronous requests, \c record has to be attached to
 * a JSEP offer (failure to do so will result in an error) and has to be
 * formatted as follows:
 *
\verbatim
{
	"request" : "record",
	"id" : <unique numeric ID for the recording; optional, will be chosen by the server if missing>
	"name" : "<Pretty name for the recording>"
}
\endverbatim
 *
 * A successful management of this request will result in a \c recording
 * event which will include the unique ID of the recording and a JSEP
 * answer to complete the setup of the associated PeerConnection to record:
 *
\verbatim
{
	"recordplay" : "event",
	"result": {
		"status" : "recording",
		"id" : <unique numeric ID>
	}
}
\endverbatim
 *
 * A \c stop request can interrupt the recording process and tear the
 * associated PeerConnection down:
 *
\verbatim
{
	"request" : "stop",
}
\endverbatim
 *
 * This will result in a \c stopped status:
 *
\verbatim
{
	"recordplay" : "event",
	"result": {
		"status" : "stopped",
		"id" : <unique numeric ID of the interrupted recording>
	}
}
\endverbatim
 *
 * For what concerns the playout, instead, the process is slightly
 * different: you first choose a recording to replay, using \c play ,
 * and then start its playout using a \c start request. Just as before,
 * a \c stop request will interrupt the playout and tear the PeerConnection
 * down. It's very important to point out that no JSEP offer must be
 * sent for replaying a recording: in this case, it will always be the
 * plugin to generate a JSON offer (in response to a \c play request),
 * which means you'll then have to provide a JSEP answer within the
 * context of the following \c start request which will close the circle.
 *
 * A \c play request has to be formatted as follows:
 *
\verbatim
{
	"request" : "play",
	"id" : <unique numeric ID of the recording to replay>
}
\endverbatim
 *
 * This will result in a \c preparing status notification which will be
 * attached to the JSEP offer originated by the plugin in order to
 * match the media available in the recording:
 *
\verbatim
{
	"recordplay" : "event",
	"result": {
		"status" : "preparing",
		"id" : <unique numeric ID of the recording>
	}
}
\endverbatim
 *
 * A \c start request, which as anticipated must be attached to the JSEP
 * answer to the previous offer sent by the plugin, has to be formatted
 * as follows:
 *
\verbatim
{
	"request" : "start",
}
\endverbatim
 *
 * This will result in a \c playing status notification:
 *
\verbatim
{
	"recordplay" : "event",
	"result": {
		"status" : "playing"
	}
}
\endverbatim
 *
 * Just as before, a \c stop request can interrupt the playout process at
 * any time, and tear the associated PeerConnection down:
 *
\verbatim
{
	"request" : "stop",
}
\endverbatim
 *
 * This will result in a \c stopped status:
 *
\verbatim
{
	"recordplay" : "event",
	"result": {
		"status" : "stopped"
	}
}
\endverbatim
 *
 * If the plugin detects a loss of the associated PeerConnection, whether
 * as a result of a \c stop request or because the 10 seconds passed, a
 * \c done result notification is triggered to inform the application
 * the recording/playout session is over:
 *
\verbatim
{
	"recordplay" : "event",
	"result": "done"
}
\endverbatim
 *
 * \ingroup plugins
 * \ref plugins
 */

#include "plugin.h"

#include <dirent.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <jansson.h>

#include "../debug.h"
#include "../apierror.h"
#include "../config.h"
#include "../mutex.h"
#include "../record.h"
#include "../sdp-utils.h"
#include "../rtp.h"
#include "../rtcp.h"
#include "../utils.h"


/* Plugin information */
#define JANUS_AUVIOUSRECORDPLAY_VERSION			5
#define JANUS_AUVIOUSRECORDPLAY_VERSION_STRING		"0.0.5"
#define JANUS_AUVIOUSRECORDPLAY_DESCRIPTION		"This is a trivial Record&Play plugin for Janus, to record WebRTC sessions and replay them."
#define JANUS_AUVIOUSRECORDPLAY_NAME				"JANUS Auvious Record&Play plugin"
#define JANUS_AUVIOUSRECORDPLAY_AUTHOR				"Meetecho s.r.l."
#define JANUS_AUVIOUSRECORDPLAY_PACKAGE			"janus.plugin.auviousrecordplay"

/* Plugin methods */
janus_plugin *create(void);
int janus_auviousrecordplay_init(janus_callbacks *callback, const char *onfig_path);
void janus_auviousrecordplay_destroy(void);
int janus_auviousrecordplay_get_api_compatibility(void);
int janus_auviousrecordplay_get_version(void);
const char *janus_auviousrecordplay_get_version_string(void);
const char *janus_auviousrecordplay_get_description(void);
const char *janus_auviousrecordplay_get_name(void);
const char *janus_auviousrecordplay_get_author(void);
const char *janus_auviousrecordplay_get_package(void);
void janus_auviousrecordplay_create_session(janus_plugin_session *handle, int *error);
struct janus_plugin_result *janus_auviousrecordplay_handle_message(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep);
void janus_auviousrecordplay_setup_media(janus_plugin_session *handle);
void janus_auviousrecordplay_incoming_rtp(janus_plugin_session *handle, janus_plugin_rtp *packet);
void janus_auviousrecordplay_incoming_rtcp(janus_plugin_session *handle, janus_plugin_rtcp *packet);
void janus_auviousrecordplay_incoming_data(janus_plugin_session *handle, janus_plugin_data *packet);
void janus_auviousrecordplay_slow_link(janus_plugin_session *handle, int uplink, int video);
void janus_auviousrecordplay_hangup_media(janus_plugin_session *handle);
void janus_auviousrecordplay_destroy_session(janus_plugin_session *handle, int *error);
json_t *janus_auviousrecordplay_query_session(janus_plugin_session *handle);

/* Plugin setup */
static janus_plugin janus_auviousrecordplay_plugin =
	JANUS_PLUGIN_INIT (
		.init = janus_auviousrecordplay_init,
		.destroy = janus_auviousrecordplay_destroy,

		.get_api_compatibility = janus_auviousrecordplay_get_api_compatibility,
		.get_version = janus_auviousrecordplay_get_version,
		.get_version_string = janus_auviousrecordplay_get_version_string,
		.get_description = janus_auviousrecordplay_get_description,
		.get_name = janus_auviousrecordplay_get_name,
		.get_author = janus_auviousrecordplay_get_author,
		.get_package = janus_auviousrecordplay_get_package,

		.create_session = janus_auviousrecordplay_create_session,
		.handle_message = janus_auviousrecordplay_handle_message,
		.setup_media = janus_auviousrecordplay_setup_media,
		.incoming_rtp = janus_auviousrecordplay_incoming_rtp,
		.incoming_rtcp = janus_auviousrecordplay_incoming_rtcp,
		.incoming_data = janus_auviousrecordplay_incoming_data,
		.slow_link = janus_auviousrecordplay_slow_link,
		.hangup_media = janus_auviousrecordplay_hangup_media,
		.destroy_session = janus_auviousrecordplay_destroy_session,
		.query_session = janus_auviousrecordplay_query_session,
	);

/* Plugin creator */
janus_plugin *create(void) {
	JANUS_LOG(LOG_VERB, "%s created!\n", JANUS_AUVIOUSRECORDPLAY_NAME);
	return &janus_auviousrecordplay_plugin;
}

/* Parameter validation */
static struct janus_json_parameter request_parameters[] = {
	{"request", JSON_STRING, JANUS_JSON_PARAM_REQUIRED}
};
static struct janus_json_parameter configure_parameters[] = {
	{"video-bitrate-max", JSON_INTEGER, JANUS_JSON_PARAM_POSITIVE},
	{"video-keyframe-interval", JSON_INTEGER, JANUS_JSON_PARAM_POSITIVE}
};
static struct janus_json_parameter record_parameters[] = {
	{"name", JSON_STRING, JANUS_JSON_PARAM_REQUIRED | JANUS_JSON_PARAM_NONEMPTY},
	{"id", JSON_INTEGER, JANUS_JSON_PARAM_POSITIVE},
	{"filename", JSON_STRING, 0},
	{"update", JANUS_JSON_BOOL, 0}
};
static struct janus_json_parameter play_parameters[] = {
	{"id", JSON_INTEGER, JANUS_JSON_PARAM_REQUIRED | JANUS_JSON_PARAM_POSITIVE},
	{"restart", JANUS_JSON_BOOL, 0}
};

/* Useful stuff */
static volatile gint initialized = 0, stopping = 0;
static gboolean notify_events = TRUE;
static janus_callbacks *gateway = NULL;
static GThread *handler_thread;
static void *janus_auviousrecordplay_handler(void *data);
static void janus_auviousrecordplay_hangup_media_internal(janus_plugin_session *handle);

typedef struct janus_auviousrecordplay_message {
	janus_plugin_session *handle;
	char *transaction;
	json_t *message;
	json_t *jsep;
} janus_auviousrecordplay_message;
static GAsyncQueue *messages = NULL;
static janus_auviousrecordplay_message exit_message;

typedef struct janus_auviousrecordplay_rtp_header_extension {
	uint16_t type;
	uint16_t length;
} janus_auviousrecordplay_rtp_header_extension;

typedef struct janus_auviousrecordplay_frame_packet {
	uint16_t seq;	/* RTP Sequence number */
	uint64_t ts;	/* RTP Timestamp */
	int len;		/* Length of the data */
	long offset;	/* Offset of the data in the file */
	struct janus_auviousrecordplay_frame_packet *next;
	struct janus_auviousrecordplay_frame_packet *prev;
} janus_auviousrecordplay_frame_packet;
janus_auviousrecordplay_frame_packet *janus_auviousrecordplay_get_frames(const char *dir, const char *filename);

typedef struct janus_auviousrecordplay_recording {
	guint64 id;					/* Recording unique ID */
	char *name;					/* Name of the recording */
	char *date;					/* Time of the recording */
	char *arc_file;				/* Audio file name */
	const char *acodec;			/* Codec used for audio, if available */
	int audio_pt;				/* Payload types to use for audio when playing recordings */
	char *vrc_file;				/* Video file name */
	const char *vcodec;			/* Codec used for video, if available */
	int video_pt;				/* Payload types to use for audio when playing recordings */
	char *offer;				/* The SDP offer that will be sent to watchers */
	GList *viewers;				/* List of users watching this recording */
	volatile gint completed;	/* Whether this recording was completed or still going on */
	volatile gint destroyed;	/* Whether this recording has been marked as destroyed */
	janus_refcount ref;			/* Reference counter */
	janus_mutex mutex;			/* Mutex for this recording */
} janus_auviousrecordplay_recording;
static GHashTable *recordings = NULL;
static janus_mutex recordings_mutex = JANUS_MUTEX_INITIALIZER;

typedef struct janus_auviousrecordplay_session {
	janus_plugin_session *handle;
	gint64 sdp_sessid;
	gint64 sdp_version;
	gboolean active;
	gboolean recorder;		/* Whether this session is used to record or to replay a WebRTC session */
	gboolean firefox;		/* We send Firefox users a different kind of FIR */
	janus_auviousrecordplay_recording *recording;
	janus_recorder *arc;	/* Audio recorder */
	janus_recorder *vrc;	/* Video recorder */
	janus_mutex rec_mutex;	/* Mutex to protect the recorders from race conditions */
	janus_auviousrecordplay_frame_packet *aframes;	/* Audio frames (for playout) */
	janus_auviousrecordplay_frame_packet *vframes;	/* Video frames (for playout) */
	guint video_remb_startup;
	gint64 video_remb_last;
	guint32 video_bitrate;
	guint video_keyframe_interval; /* keyframe request interval (ms) */
	guint64 video_keyframe_request_last; /* timestamp of last keyframe request sent */
	gint video_fir_seq;
	guint32 simulcast_ssrc;	/* We don't support Simulcast in this plugin, so we'll stick to the base substream */
	volatile gint hangingup;
	volatile gint destroyed;
	janus_refcount ref;
} janus_auviousrecordplay_session;
static GHashTable *sessions;
static janus_mutex sessions_mutex = JANUS_MUTEX_INITIALIZER;

static void janus_auviousrecordplay_session_destroy(janus_auviousrecordplay_session *session) {
	if(session && g_atomic_int_compare_and_exchange(&session->destroyed, 0, 1))
		janus_refcount_decrease(&session->ref);
}

static void janus_auviousrecordplay_session_free(const janus_refcount *session_ref) {
	janus_auviousrecordplay_session *session = janus_refcount_containerof(session_ref, janus_auviousrecordplay_session, ref);
	/* Remove the reference to the core plugin session */
	janus_refcount_decrease(&session->handle->ref);
	/* This session can be destroyed, free all the resources */
	g_free(session);
}


static void janus_auviousrecordplay_recording_destroy(janus_auviousrecordplay_recording *recording) {
	if(recording && g_atomic_int_compare_and_exchange(&recording->destroyed, 0, 1))
		janus_refcount_decrease(&recording->ref);
}

static void janus_auviousrecordplay_recording_free(const janus_refcount *recording_ref) {
	janus_auviousrecordplay_recording *recording = janus_refcount_containerof(recording_ref, janus_auviousrecordplay_recording, ref);
	/* This recording can be destroyed, free all the resources */
	g_free(recording->name);
	g_free(recording->date);
	g_free(recording->arc_file);
	g_free(recording->vrc_file);
	g_free(recording->offer);
	g_free(recording);
}


static char *recordings_path = NULL;
static void *janus_auviousrecordplay_playout_thread(void *data);

/* Helper to send RTCP feedback back to recorders, if needed */
void janus_auviousrecordplay_send_rtcp_feedback(janus_plugin_session *handle, int video, char *buf, int len);

/* To make things easier, we use static payload types for viewers (unless it's for G.711 or G.722) */
#define AUDIO_PT		111
#define VIDEO_PT		100

/* Helper method to check which codec was used in a specific recording */
static const char *janus_auviousrecordplay_parse_codec(const char *dir, const char *filename) {
	if(dir == NULL || filename == NULL)
		return NULL;
	char source[1024];
	if(strstr(filename, ".mjr"))
		g_snprintf(source, 1024, "%s/%s", dir, filename);
	else
		g_snprintf(source, 1024, "%s/%s.mjr", dir, filename);
	FILE *file = fopen(source, "rb");
	if(file == NULL) {
		JANUS_LOG(LOG_ERR, "Could not open file %s\n", source);
		return NULL;
	}
	fseek(file, 0L, SEEK_END);
	long fsize = ftell(file);
	fseek(file, 0L, SEEK_SET);

	/* Pre-parse */
	JANUS_LOG(LOG_VERB, "Pre-parsing file %s to generate ordered index...\n", source);
	gboolean parsed_header = FALSE;
	int bytes = 0;
	long offset = 0;
	uint16_t len = 0;
	char prebuffer[1500];
	memset(prebuffer, 0, 1500);
	/* Let's look for timestamp resets first */
	while(offset < fsize) {
		/* Read frame header */
		fseek(file, offset, SEEK_SET);
		bytes = fread(prebuffer, sizeof(char), 8, file);
		if(bytes != 8 || prebuffer[0] != 'M') {
			JANUS_LOG(LOG_ERR, "Invalid header...\n");
			fclose(file);
			return NULL;
		}
		if(prebuffer[1] == 'E') {
			/* Either the old .mjr format header ('MEETECHO' header followed by 'audio' or 'video'), or a frame */
			offset += 8;
			bytes = fread(&len, sizeof(uint16_t), 1, file);
			len = ntohs(len);
			offset += 2;
			if(len == 5 && !parsed_header) {
				/* This is the main header */
				parsed_header = TRUE;
				bytes = fread(prebuffer, sizeof(char), 5, file);
				if(prebuffer[0] == 'v') {
					JANUS_LOG(LOG_VERB, "This is an old video recording, assuming VP8\n");
					fclose(file);
					return "vp8";
				} else if(prebuffer[0] == 'a') {
					JANUS_LOG(LOG_VERB, "This is an old audio recording, assuming Opus\n");
					fclose(file);
					return "opus";
				}
			}
			JANUS_LOG(LOG_WARN, "Unsupported recording media type...\n");
			fclose(file);
			return NULL;
		} else if(prebuffer[1] == 'J') {
			/* New .mjr format */
			offset += 8;
			bytes = fread(&len, sizeof(uint16_t), 1, file);
			len = ntohs(len);
			offset += 2;
			if(len > 0 && !parsed_header) {
				/* This is the info header */
				bytes = fread(prebuffer, sizeof(char), len, file);
				if(bytes < 0) {
					JANUS_LOG(LOG_ERR, "Error reading from file... %s\n", strerror(errno));
					fclose(file);
					return NULL;
				}
				parsed_header = TRUE;
				prebuffer[len] = '\0';
				json_error_t error;
				json_t *info = json_loads(prebuffer, 0, &error);
				if(!info) {
					JANUS_LOG(LOG_ERR, "JSON error: on line %d: %s\n", error.line, error.text);
					JANUS_LOG(LOG_WARN, "Error parsing info header...\n");
					fclose(file);
					return NULL;
				}
				/* Is it audio or video? */
				json_t *type = json_object_get(info, "t");
				if(!type || !json_is_string(type)) {
					JANUS_LOG(LOG_WARN, "Missing/invalid recording type in info header...\n");
					json_decref(info);
					fclose(file);
					return NULL;
				}
				const char *t = json_string_value(type);
				gboolean video = FALSE;
				if(!strcasecmp(t, "v")) {
					video = TRUE;
				} else if(!strcasecmp(t, "a")) {
					video = FALSE;
				} else {
					JANUS_LOG(LOG_WARN, "Unsupported recording type '%s' in info header...\n", t);
					json_decref(info);
					fclose(file);
					return NULL;
				}
				/* What codec was used? */
				json_t *codec = json_object_get(info, "c");
				if(!codec || !json_is_string(codec)) {
					JANUS_LOG(LOG_WARN, "Missing recording codec in info header...\n");
					json_decref(info);
					fclose(file);
					return NULL;
				}
				const char *c = json_string_value(codec);
				const char *mcodec = janus_sdp_match_preferred_codec(video ? JANUS_SDP_VIDEO : JANUS_SDP_AUDIO, (char *)c);
				if(mcodec != NULL) {
					/* Found! */
					json_decref(info);
					fclose(file);
					return mcodec;
				}
				json_decref(info);
			}
			JANUS_LOG(LOG_WARN, "No codec found...\n");
			fclose(file);
			return NULL;
		} else {
			JANUS_LOG(LOG_ERR, "Invalid header...\n");
			fclose(file);
			return NULL;
		}
	}
	fclose(file);
	return NULL;
}

/* Helper method to prepare an SDP offer when a recording is available */
static int janus_auviousrecordplay_generate_offer(janus_auviousrecordplay_recording *rec) {
	if(rec == NULL)
		return -1;
	/* Prepare an SDP offer we'll send to playout viewers */
	gboolean offer_audio = (rec->arc_file != NULL && rec->acodec != NULL),
		offer_video = (rec->vrc_file != NULL && rec->vcodec != NULL);
	char s_name[100];
	g_snprintf(s_name, sizeof(s_name), "Recording %"SCNu64, rec->id);
	janus_sdp *offer = janus_sdp_generate_offer(
		s_name, "1.1.1.1",
		JANUS_SDP_OA_AUDIO, offer_audio,
		JANUS_SDP_OA_AUDIO_CODEC, rec->acodec,
		JANUS_SDP_OA_AUDIO_PT, rec->audio_pt,
		JANUS_SDP_OA_AUDIO_DIRECTION, JANUS_SDP_SENDONLY,
		JANUS_SDP_OA_VIDEO, offer_video,
		JANUS_SDP_OA_VIDEO_CODEC, rec->vcodec,
		JANUS_SDP_OA_VIDEO_PT, rec->video_pt,
		JANUS_SDP_OA_VIDEO_DIRECTION, JANUS_SDP_SENDONLY,
		JANUS_SDP_OA_DATA, FALSE,
		JANUS_SDP_OA_DONE);
	g_free(rec->offer);
	rec->offer = janus_sdp_write(offer);
	janus_sdp_destroy(offer);
	return 0;
}

static void janus_auviousrecordplay_message_free(janus_auviousrecordplay_message *msg) {
	if(!msg || msg == &exit_message)
		return;

	if(msg->handle && msg->handle->plugin_handle) {
		janus_auviousrecordplay_session *session = (janus_auviousrecordplay_session *)msg->handle->plugin_handle;
		janus_refcount_decrease(&session->ref);
	}
	msg->handle = NULL;

	g_free(msg->transaction);
	msg->transaction = NULL;
	if(msg->message)
		json_decref(msg->message);
	msg->message = NULL;
	if(msg->jsep)
		json_decref(msg->jsep);
	msg->jsep = NULL;

	g_free(msg);
}


/* Error codes */
#define JANUS_AUVIOUSRECORDPLAY_ERROR_NO_MESSAGE			411
#define JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_JSON			412
#define JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_REQUEST		413
#define JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_ELEMENT		414
#define JANUS_AUVIOUSRECORDPLAY_ERROR_MISSING_ELEMENT		415
#define JANUS_AUVIOUSRECORDPLAY_ERROR_NOT_FOUND			416
#define JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_RECORDING	417
#define JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_STATE		418
#define JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_SDP			419
#define JANUS_AUVIOUSRECORDPLAY_ERROR_RECORDING_EXISTS		420
#define JANUS_AUVIOUSRECORDPLAY_ERROR_UNKNOWN_ERROR		499


/* Plugin implementation */
int janus_auviousrecordplay_init(janus_callbacks *callback, const char *config_path) {
	if(g_atomic_int_get(&stopping)) {
		/* Still stopping from before */
		return -1;
	}
	if(callback == NULL || config_path == NULL) {
		/* Invalid arguments */
		return -1;
	}

	/* Read configuration */
	char filename[255];
	g_snprintf(filename, 255, "%s/%s.cfg", config_path, JANUS_AUVIOUSRECORDPLAY_PACKAGE);
	JANUS_LOG(LOG_VERB, "Configuration file: %s\n", filename);
	janus_config *config = janus_config_parse(filename);
	if(config != NULL)
		janus_config_print(config);
	/* Parse configuration */
	if(config != NULL) {
		janus_config_category *config_general = janus_config_get_create(config, NULL, janus_config_type_category, "general");
		janus_config_item *path = janus_config_get(config, config_general, janus_config_type_item, "path");
		if(path && path->value)
			recordings_path = g_strdup(path->value);
		janus_config_item *events = janus_config_get(config, config_general, janus_config_type_item, "events");
		if(events != NULL && events->value != NULL)
			notify_events = janus_is_true(events->value);
		if(!notify_events && callback->events_is_enabled()) {
			JANUS_LOG(LOG_WARN, "Notification of events to handlers disabled for %s\n", JANUS_AUVIOUSRECORDPLAY_NAME);
		}
		/* Done */
		janus_config_destroy(config);
		config = NULL;
	}
	if(recordings_path == NULL) {
		JANUS_LOG(LOG_FATAL, "No recordings path specified, giving up...\n");
		return -1;
	}
	/* Create the folder, if needed */
	struct stat st = {0};
	if(stat(recordings_path, &st) == -1) {
		int res = janus_mkdir(recordings_path, 0755);
		JANUS_LOG(LOG_VERB, "Creating folder: %d\n", res);
		if(res != 0) {
			JANUS_LOG(LOG_ERR, "%s", strerror(errno));
			return -1;	/* No point going on... */
		}
	}
	recordings = g_hash_table_new_full(g_int64_hash, g_int64_equal, (GDestroyNotify)g_free, (GDestroyNotify)janus_auviousrecordplay_recording_destroy);

	sessions = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)janus_auviousrecordplay_session_destroy);
	messages = g_async_queue_new_full((GDestroyNotify) janus_auviousrecordplay_message_free);
	/* This is the callback we'll need to invoke to contact the gateway */
	gateway = callback;

	g_atomic_int_set(&initialized, 1);

	/* Launch the thread that will handle incoming messages */
	GError *error = NULL;
	handler_thread = g_thread_try_new("recordplay handler", janus_auviousrecordplay_handler, NULL, &error);
	if(error != NULL) {
		g_atomic_int_set(&initialized, 0);
		JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the Record&Play handler thread...\n", error->code, error->message ? error->message : "??");
		return -1;
	}
	JANUS_LOG(LOG_INFO, "%s initialized!\n", JANUS_AUVIOUSRECORDPLAY_NAME);
	return 0;
}

void janus_auviousrecordplay_destroy(void) {
	if(!g_atomic_int_get(&initialized))
		return;
	g_atomic_int_set(&stopping, 1);

	g_async_queue_push(messages, &exit_message);
	if(handler_thread != NULL) {
		g_thread_join(handler_thread);
		handler_thread = NULL;
	}
	/* FIXME We should destroy the sessions cleanly */
	janus_mutex_lock(&sessions_mutex);
	g_hash_table_destroy(sessions);
	g_hash_table_destroy(recordings);
	janus_mutex_unlock(&sessions_mutex);
	g_async_queue_unref(messages);
	messages = NULL;
	sessions = NULL;
	g_atomic_int_set(&initialized, 0);
	g_atomic_int_set(&stopping, 0);
	JANUS_LOG(LOG_INFO, "%s destroyed!\n", JANUS_AUVIOUSRECORDPLAY_NAME);
}

int janus_auviousrecordplay_get_api_compatibility(void) {
	/* Important! This is what your plugin MUST always return: don't lie here or bad things will happen */
	return JANUS_PLUGIN_API_VERSION;
}

int janus_auviousrecordplay_get_version(void) {
	return JANUS_AUVIOUSRECORDPLAY_VERSION;
}

const char *janus_auviousrecordplay_get_version_string(void) {
	return JANUS_AUVIOUSRECORDPLAY_VERSION_STRING;
}

const char *janus_auviousrecordplay_get_description(void) {
	return JANUS_AUVIOUSRECORDPLAY_DESCRIPTION;
}

const char *janus_auviousrecordplay_get_name(void) {
	return JANUS_AUVIOUSRECORDPLAY_NAME;
}

const char *janus_auviousrecordplay_get_author(void) {
	return JANUS_AUVIOUSRECORDPLAY_AUTHOR;
}

const char *janus_auviousrecordplay_get_package(void) {
	return JANUS_AUVIOUSRECORDPLAY_PACKAGE;
}

static janus_auviousrecordplay_session *janus_auviousrecordplay_lookup_session(janus_plugin_session *handle) {
	janus_auviousrecordplay_session *session = NULL;
	if (g_hash_table_contains(sessions, handle)) {
		session = (janus_auviousrecordplay_session *)handle->plugin_handle;
	}
	return session;
}

void janus_auviousrecordplay_create_session(janus_plugin_session *handle, int *error) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
		*error = -1;
		return;
	}
	janus_auviousrecordplay_session *session = g_malloc0(sizeof(janus_auviousrecordplay_session));
	session->handle = handle;
	session->active = FALSE;
	session->recorder = FALSE;
	session->firefox = FALSE;
	session->arc = NULL;
	session->vrc = NULL;
	janus_mutex_init(&session->rec_mutex);
	g_atomic_int_set(&session->hangingup, 0);
	g_atomic_int_set(&session->destroyed, 0);
	session->video_remb_startup = 4;
	session->video_remb_last = janus_get_monotonic_time();
	session->video_bitrate = 1024 * 1024; 		/* This is 1mbps by default */
	session->video_keyframe_request_last = 0;
	session->video_keyframe_interval = 15000; 	/* 15 seconds by default */
	session->video_fir_seq = 0;
	janus_refcount_init(&session->ref, janus_auviousrecordplay_session_free);
	handle->plugin_handle = session;

	janus_mutex_lock(&sessions_mutex);
	g_hash_table_insert(sessions, handle, session);
	janus_mutex_unlock(&sessions_mutex);

	return;
}

void janus_auviousrecordplay_destroy_session(janus_plugin_session *handle, int *error) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
		*error = -1;
		return;
	}
	janus_mutex_lock(&sessions_mutex);
	janus_auviousrecordplay_session *session = janus_auviousrecordplay_lookup_session(handle);
	if(!session) {
		janus_mutex_unlock(&sessions_mutex);
		JANUS_LOG(LOG_ERR, "No Record&Play session associated with this handle...\n");
		*error = -2;
		return;
	}
	JANUS_LOG(LOG_VERB, "Removing Record&Play session...\n");
	janus_auviousrecordplay_hangup_media_internal(handle);
	g_hash_table_remove(sessions, handle);
	janus_mutex_unlock(&sessions_mutex);
	return;
}

json_t *janus_auviousrecordplay_query_session(janus_plugin_session *handle) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
		return NULL;
	}
	janus_mutex_lock(&sessions_mutex);
	janus_auviousrecordplay_session *session = janus_auviousrecordplay_lookup_session(handle);
	if(!session) {
		janus_mutex_unlock(&sessions_mutex);
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return NULL;
	}
	janus_refcount_increase(&session->ref);
	janus_mutex_unlock(&sessions_mutex);
	/* In the echo test, every session is the same: we just provide some configure info */
	json_t *info = json_object();
	json_object_set_new(info, "type", json_string(session->recorder ? "recorder" : (session->recording ? "player" : "none")));
	if(session->recording) {
		janus_refcount_increase(&session->recording->ref);
		json_object_set_new(info, "recording_id", json_integer(session->recording->id));
		json_object_set_new(info, "recording_name", json_string(session->recording->name));
		janus_refcount_decrease(&session->recording->ref);
	}
	json_object_set_new(info, "hangingup", json_integer(g_atomic_int_get(&session->hangingup)));
	json_object_set_new(info, "destroyed", json_integer(g_atomic_int_get(&session->destroyed)));
	janus_refcount_decrease(&session->ref);
	return info;
}

struct janus_plugin_result *janus_auviousrecordplay_handle_message(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return janus_plugin_result_new(JANUS_PLUGIN_ERROR, g_atomic_int_get(&stopping) ? "Shutting down" : "Plugin not initialized", NULL);

	/* Pre-parse the message */
	int error_code = 0;
	char error_cause[512];
	json_t *root = message;
	json_t *response = NULL;

	janus_mutex_lock(&sessions_mutex);
	janus_auviousrecordplay_session *session = janus_auviousrecordplay_lookup_session(handle);
	if(!session) {
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_UNKNOWN_ERROR;
		g_snprintf(error_cause, 512, "%s", "session associated with this handle...");
		goto plugin_response;
	}
	/* Increase the reference counter for this session: we'll decrease it after we handle the message */
	janus_refcount_increase(&session->ref);
	janus_mutex_unlock(&sessions_mutex);
	if(g_atomic_int_get(&session->destroyed)) {
		JANUS_LOG(LOG_ERR, "Session has already been destroyed...\n");
		error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_UNKNOWN_ERROR;
		g_snprintf(error_cause, 512, "%s", "Session has already been destroyed...");
		goto plugin_response;
	}

	if(message == NULL) {
		JANUS_LOG(LOG_ERR, "No message??\n");
		error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_NO_MESSAGE;
		g_snprintf(error_cause, 512, "%s", "No message??");
		goto plugin_response;
	}
	if(!json_is_object(root)) {
		JANUS_LOG(LOG_ERR, "JSON error: not an object\n");
		error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_JSON;
		g_snprintf(error_cause, 512, "JSON error: not an object");
		goto plugin_response;
	}
	/* Get the request first */
	JANUS_VALIDATE_JSON_OBJECT(root, request_parameters,
		error_code, error_cause, TRUE,
		JANUS_AUVIOUSRECORDPLAY_ERROR_MISSING_ELEMENT, JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_ELEMENT);
	if(error_code != 0)
		goto plugin_response;
	json_t *request = json_object_get(root, "request");
	/* Some requests ('create' and 'destroy') can be handled synchronously */
	const char *request_text = json_string_value(request);
	if(!strcasecmp(request_text, "update")) {
		/* Nothing to do here just send info back */
		response = json_object();
		json_object_set_new(response, "recordplay", json_string("ok"));
		goto plugin_response;
	} else if(!strcasecmp(request_text, "list")) {
		json_t *list = json_array();
		JANUS_LOG(LOG_VERB, "Request for the list of recordings\n");
		/* Return a list of all available recordings */
		janus_mutex_lock(&recordings_mutex);
		GHashTableIter iter;
		gpointer value;
		g_hash_table_iter_init(&iter, recordings);
		while (g_hash_table_iter_next(&iter, NULL, &value)) {
			janus_auviousrecordplay_recording *rec = value;
			if(!g_atomic_int_get(&rec->completed))	/* Ongoing recording, skip */
				continue;
			janus_refcount_increase(&rec->ref);
			json_t *ml = json_object();
			json_object_set_new(ml, "id", json_integer(rec->id));
			json_object_set_new(ml, "name", json_string(rec->name));
			json_object_set_new(ml, "date", json_string(rec->date));
			json_object_set_new(ml, "audio", rec->arc_file ? json_true() : json_false());
			if(rec->acodec)
				json_object_set_new(ml, "audio_codec", json_string(rec->acodec));
			json_object_set_new(ml, "video", rec->vrc_file ? json_true() : json_false());
			if(rec->vcodec)
				json_object_set_new(ml, "video_codec", json_string(rec->vcodec));
			janus_refcount_decrease(&rec->ref);
			json_array_append_new(list, ml);
		}
		janus_mutex_unlock(&recordings_mutex);
		/* Send info back */
		response = json_object();
		json_object_set_new(response, "recordplay", json_string("list"));
		json_object_set_new(response, "list", list);
		goto plugin_response;
	} else if(!strcasecmp(request_text, "configure")) {
		JANUS_VALIDATE_JSON_OBJECT(root, configure_parameters,
			error_code, error_cause, TRUE,
			JANUS_AUVIOUSRECORDPLAY_ERROR_MISSING_ELEMENT, JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_ELEMENT);
		if(error_code != 0)
			goto plugin_response;
		json_t *video_bitrate_max = json_object_get(root, "video-bitrate-max");
		if(video_bitrate_max) {
			session->video_bitrate = json_integer_value(video_bitrate_max);
			JANUS_LOG(LOG_VERB, "Video bitrate has been set to %"SCNu32"\n", session->video_bitrate);
		}
		json_t *video_keyframe_interval= json_object_get(root, "video-keyframe-interval");
		if(video_keyframe_interval) {
			session->video_keyframe_interval = json_integer_value(video_keyframe_interval);
			JANUS_LOG(LOG_VERB, "Video keyframe interval has been set to %u\n", session->video_keyframe_interval);
		}
		response = json_object();
		json_object_set_new(response, "recordplay", json_string("configure"));
		json_object_set_new(response, "status", json_string("ok"));
		/* Return a success, and also let the client be aware of what changed, to allow crosschecks */
		json_t *settings = json_object();
		json_object_set_new(settings, "video-keyframe-interval", json_integer(session->video_keyframe_interval));
		json_object_set_new(settings, "video-bitrate-max", json_integer(session->video_bitrate));
		json_object_set_new(response, "settings", settings);
		goto plugin_response;
	} else if(!strcasecmp(request_text, "record") || !strcasecmp(request_text, "play")
			|| !strcasecmp(request_text, "start") || !strcasecmp(request_text, "stop")
			|| !strcasecmp(request_text, "record-generate-offer")
			|| !strcasecmp(request_text, "record-process-answer")) {
		/* These messages are handled asynchronously */
		janus_auviousrecordplay_message *msg = g_malloc(sizeof(janus_auviousrecordplay_message));
		msg->handle = handle;
		msg->transaction = transaction;
		msg->message = root;
		msg->jsep = jsep;

		g_async_queue_push(messages, msg);

		return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
	} else {
		JANUS_LOG(LOG_VERB, "Unknown request '%s'\n", request_text);
		error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_REQUEST;
		g_snprintf(error_cause, 512, "Unknown request '%s'", request_text);
	}

plugin_response:
		{
			if(error_code == 0 && !response) {
				error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_UNKNOWN_ERROR;
				g_snprintf(error_cause, 512, "Invalid response");
			}
			if(error_code != 0) {
				/* Prepare JSON error event */
				json_t *event = json_object();
				json_object_set_new(event, "recordplay", json_string("event"));
				json_object_set_new(event, "error_code", json_integer(error_code));
				json_object_set_new(event, "error", json_string(error_cause));
				response = event;
			}
			if(root != NULL)
				json_decref(root);
			if(jsep != NULL)
				json_decref(jsep);
			g_free(transaction);

			if(session != NULL)
				janus_refcount_decrease(&session->ref);
			return janus_plugin_result_new(JANUS_PLUGIN_OK, NULL, response);
		}

}

void janus_auviousrecordplay_setup_media(janus_plugin_session *handle) {
	JANUS_LOG(LOG_INFO, "[%s-%p] WebRTC media is now available\n", JANUS_AUVIOUSRECORDPLAY_PACKAGE, handle);
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	janus_mutex_lock(&sessions_mutex);
	janus_auviousrecordplay_session *session = janus_auviousrecordplay_lookup_session(handle);
	if(!session) {
		janus_mutex_unlock(&sessions_mutex);
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return;
	}
	if(g_atomic_int_get(&session->destroyed)) {
		janus_mutex_unlock(&sessions_mutex);
		return;
	}
	janus_refcount_increase(&session->ref);
	janus_mutex_unlock(&sessions_mutex);
	g_atomic_int_set(&session->hangingup, 0);
	/* Take note of the fact that the session is now active */
	session->active = TRUE;
	if(!session->recorder) {
		GError *error = NULL;
		janus_refcount_increase(&session->ref);
		g_thread_try_new("recordplay playout thread", &janus_auviousrecordplay_playout_thread, session, &error);
		if(error != NULL) {
			janus_refcount_decrease(&session->ref);
			/* FIXME Should we notify this back to the user somehow? */
			JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the Record&Play playout thread...\n", error->code, error->message ? error->message : "??");
			if(gateway) {
				gateway->close_pc(session->handle);
			}
		}
	}
	janus_refcount_decrease(&session->ref);
}

void janus_auviousrecordplay_send_rtcp_feedback(janus_plugin_session *handle, int video, char *buf, int len) {
	if(video != 1)
		return;	/* We just do this for video, for now */

	janus_auviousrecordplay_session *session = (janus_auviousrecordplay_session *)handle->plugin_handle;

	/* Send a RR+SDES+REMB every five seconds, or ASAP while we are still
	 * ramping up (first 4 RTP packets) */
	gint64 now = janus_get_monotonic_time();
	gint64 elapsed = now - session->video_remb_last;
	gboolean remb_rampup = session->video_remb_startup > 0;

	if(remb_rampup || (elapsed >= 5*G_USEC_PER_SEC)) {
		guint32 bitrate = session->video_bitrate;

		if(remb_rampup) {
			bitrate = bitrate / session->video_remb_startup;
			session->video_remb_startup--;
		}

		/* Send a new REMB back */
    gateway->send_remb(handle, bitrate);

		session->video_remb_last = now;
	}

	/* Request a keyframe on a regular basis (every session->video_keyframe_interval ms) */
	elapsed = now - session->video_keyframe_request_last;
	gint64 interval = (gint64)(session->video_keyframe_interval / 1000) * G_USEC_PER_SEC;

	if(elapsed >= interval) {
    /* Send a PLI */
    gateway->send_pli(handle);
    session->video_keyframe_request_last = now;
	}
}

void janus_auviousrecordplay_incoming_rtp(janus_plugin_session *handle, janus_plugin_rtp *packet) {
	if(handle == NULL || g_atomic_int_get(&handle->stopped) || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	if(gateway) {
		janus_auviousrecordplay_session *session = (janus_auviousrecordplay_session *)handle->plugin_handle;
		if(!session) {
			JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
			return;
		}
		if(g_atomic_int_get(&session->destroyed))
			return;
    
    gboolean video = packet->video;
    char *buf = packet->buffer;
    uint16_t len = packet->length;

		if(video && session->simulcast_ssrc) {
			/* The user is simulcasting: drop everything except the base layer */
			janus_rtp_header *header = (janus_rtp_header *)buf;
			uint32_t ssrc = ntohl(header->ssrc);
			if(ssrc != session->simulcast_ssrc) {
				JANUS_LOG(LOG_DBG, "Dropping packet (not base simulcast substream)\n");
				return;
			}
		}
		/* Are we recording? */
		if(session->recorder) {
			janus_recorder_save_frame(video ? session->vrc : session->arc, buf, len);
		}

		janus_auviousrecordplay_send_rtcp_feedback(handle, video, buf, len);
	}
}

void janus_auviousrecordplay_incoming_rtcp(janus_plugin_session *handle, janus_plugin_rtcp *packet) {
	if(handle == NULL || g_atomic_int_get(&handle->stopped) || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
}

void janus_auviousrecordplay_incoming_data(janus_plugin_session *handle, janus_plugin_data *packet) {
	if(handle == NULL || g_atomic_int_get(&handle->stopped) || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	/* FIXME We don't care */
}

void janus_auviousrecordplay_slow_link(janus_plugin_session *handle, int uplink, int video) {
	if(handle == NULL || g_atomic_int_get(&handle->stopped) || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized) || !gateway)
		return;

	janus_mutex_lock(&sessions_mutex);
	janus_auviousrecordplay_session *session = janus_auviousrecordplay_lookup_session(handle);
	if(!session || g_atomic_int_get(&session->destroyed)) {
		janus_mutex_unlock(&sessions_mutex);
		return;
	}
	janus_refcount_increase(&session->ref);
	janus_mutex_unlock(&sessions_mutex);

	json_t *event = json_object();
	json_object_set_new(event, "recordplay", json_string("event"));
	json_t *result = json_object();
	json_object_set_new(result, "status", json_string("slow_link"));
	/* What is uplink for the server is downlink for the client, so turn the tables */
	json_object_set_new(result, "current-bitrate", json_integer(session->video_bitrate));
	json_object_set_new(result, "uplink", json_integer(uplink ? 0 : 1));
	json_object_set_new(event, "result", result);
	gateway->push_event(session->handle, &janus_auviousrecordplay_plugin, NULL, event, NULL);
	json_decref(event);
	janus_refcount_decrease(&session->ref);
}

void janus_auviousrecordplay_hangup_media(janus_plugin_session *handle) {
	JANUS_LOG(LOG_INFO, "[%s-%p] No WebRTC media anymore\n", JANUS_AUVIOUSRECORDPLAY_PACKAGE, handle);
	janus_mutex_lock(&sessions_mutex);
	janus_auviousrecordplay_hangup_media_internal(handle);
	janus_mutex_unlock(&sessions_mutex);
}

static void janus_auviousrecordplay_recordings_remove(janus_auviousrecordplay_recording *rec) {
	janus_mutex_lock(&recordings_mutex);
	if (g_hash_table_remove(recordings, &rec->id)) {
		JANUS_LOG(LOG_VERB, "[%s] Recording removed from list.\n", JANUS_AUVIOUSRECORDPLAY_PACKAGE);
	}
	janus_mutex_unlock(&recordings_mutex);
}

static void janus_auviousrecordplay_hangup_media_internal(janus_plugin_session *handle) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	janus_auviousrecordplay_session *session = janus_auviousrecordplay_lookup_session(handle);
	if(!session) {
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return;
	}
	session->active = FALSE;
	if(g_atomic_int_get(&session->destroyed)) {
		return;
	}
	if(g_atomic_int_add(&session->hangingup, 1)) {
		return;
	}
	session->simulcast_ssrc = 0;

	/* Send an event to the browser and tell it's over */
	json_t *event = json_object();
	json_object_set_new(event, "recordplay", json_string("event"));
	json_object_set_new(event, "result", json_string("done"));
	int ret = gateway->push_event(handle, &janus_auviousrecordplay_plugin, NULL, event, NULL);
	JANUS_LOG(LOG_VERB, "  >> Pushing event: %d (%s)\n", ret, janus_get_api_error(ret));
	json_decref(event);

	session->active = FALSE;
	janus_mutex_lock(&session->rec_mutex);
	if(session->arc) {
		janus_recorder_close(session->arc);
		JANUS_LOG(LOG_INFO, "Closed audio recording %s\n", session->arc->filename ? session->arc->filename : "??");
		janus_recorder_destroy(session->arc);
	}
	session->arc = NULL;
	if(session->vrc) {
		janus_recorder_close(session->vrc);
		JANUS_LOG(LOG_INFO, "Closed video recording %s\n", session->vrc->filename ? session->vrc->filename : "??");
		janus_recorder_destroy(session->vrc);
	}
	session->vrc = NULL;
	janus_mutex_unlock(&session->rec_mutex);
	if(session->recording) {
		janus_auviousrecordplay_recordings_remove(session->recording);
		janus_refcount_decrease(&session->recording->ref);
		session->recording = NULL;
	}
}

/* Thread to handle incoming messages */
static void *janus_auviousrecordplay_handler(void *data) {
	JANUS_LOG(LOG_VERB, "Joining Record&Play handler thread\n");
	janus_auviousrecordplay_message *msg = NULL;
	int error_code = 0;
	char error_cause[512];
	json_t *root = NULL;
	while(g_atomic_int_get(&initialized) && !g_atomic_int_get(&stopping)) {
		msg = g_async_queue_pop(messages);
		if(msg == &exit_message)
			break;
		if(msg->handle == NULL) {
			janus_auviousrecordplay_message_free(msg);
			continue;
		}
		janus_mutex_lock(&sessions_mutex);
		janus_auviousrecordplay_session *session = janus_auviousrecordplay_lookup_session(msg->handle);
		if(!session) {
			janus_mutex_unlock(&sessions_mutex);
			JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
			janus_auviousrecordplay_message_free(msg);
			continue;
		}
		if(g_atomic_int_get(&session->destroyed)) {
			janus_mutex_unlock(&sessions_mutex);
			janus_auviousrecordplay_message_free(msg);
			continue;
		}
		janus_mutex_unlock(&sessions_mutex);
		/* Handle request */
		error_code = 0;
		root = NULL;
		if(msg->message == NULL) {
			JANUS_LOG(LOG_ERR, "No message??\n");
			error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_NO_MESSAGE;
			g_snprintf(error_cause, 512, "%s", "No message??");
			goto error;
		}
		root = msg->message;
		/* Get the request first */
		JANUS_VALIDATE_JSON_OBJECT(root, request_parameters,
			error_code, error_cause, TRUE,
			JANUS_AUVIOUSRECORDPLAY_ERROR_MISSING_ELEMENT, JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_ELEMENT);
		if(error_code != 0)
			goto error;
		const char *msg_sdp_type = json_string_value(json_object_get(msg->jsep, "type"));
		const char *msg_sdp = json_string_value(json_object_get(msg->jsep, "sdp"));
		json_t *request = json_object_get(root, "request");
		const char *request_text = json_string_value(request);

		JANUS_LOG(LOG_VERB, "  >> Handling request type: %s\n", request_text); 

		json_t *event = NULL;
		json_t *result = NULL;
		char *sdp = NULL;
		gboolean sdp_update = FALSE;
		if(json_object_get(msg->jsep, "update") != NULL)
			sdp_update = json_is_true(json_object_get(msg->jsep, "update"));
		const char *filename_text = NULL;
		if(!strcasecmp(request_text, "record")) {
			if(!msg_sdp || !msg_sdp_type || strcasecmp(msg_sdp_type, "offer")) {
				JANUS_LOG(LOG_ERR, "Missing SDP offer\n");
				error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_MISSING_ELEMENT;
				g_snprintf(error_cause, 512, "Missing SDP offer");
				goto error;
			}
			JANUS_VALIDATE_JSON_OBJECT(root, record_parameters,
				error_code, error_cause, TRUE,
				JANUS_AUVIOUSRECORDPLAY_ERROR_MISSING_ELEMENT, JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_ELEMENT);
			if(error_code != 0)
				goto error;
			char error_str[512];
			janus_sdp *offer = janus_sdp_parse(msg_sdp, error_str, sizeof(error_str)), *answer = NULL;
			if(offer == NULL) {
				json_decref(event);
				JANUS_LOG(LOG_ERR, "Error parsing offer: %s\n", error_str);
				error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_SDP;
				g_snprintf(error_cause, 512, "Error parsing offer: %s", error_str);
				goto error;
			}
			json_t *name = json_object_get(root, "name");
			const char *name_text = json_string_value(name);
			json_t *filename = json_object_get(root, "filename");
			if(filename) {
				filename_text = json_string_value(filename);
			}
			json_t *update = json_object_get(root, "update");
			gboolean do_update = update ? json_is_true(update) : FALSE;
			if(do_update && !sdp_update) {
				JANUS_LOG(LOG_WARN, "Got a 'update' request, but no SDP update? Ignoring...\n");
			}
			/* Check if this is a new recorder, or if an update is taking place (i.e., ICE restart) */
			guint64 id = 0;
			janus_auviousrecordplay_recording *rec = NULL;
			gboolean audio = FALSE, video = FALSE;
			if(sdp_update) {
				/* Renegotiation: make sure the user provided an offer, and send answer */
				JANUS_LOG(LOG_VERB, "Request to update existing recorder\n");
				if(!session->recorder || !session->recording) {
					JANUS_LOG(LOG_ERR, "Not a recording session, can't update\n");
					error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_STATE;
					g_snprintf(error_cause, 512, "Not a recording session, can't update");
					goto error;
				}
				id = session->recording->id;
				rec = session->recording;
				session->sdp_version++;		/* This needs to be increased when it changes */
				audio = (session->arc != NULL);
				video = (session->vrc != NULL);
				sdp_update = do_update;
				goto recdone;
			}
			/* If we're here, we're doing a new recording */
			janus_mutex_lock(&recordings_mutex);
			json_t *rec_id = json_object_get(root, "id");
			if(rec_id) {
				id = json_integer_value(rec_id);
				if(id > 0) {
					/* Let's make sure the ID doesn't exist already */
					if(g_hash_table_lookup(recordings, &id) != NULL) {
						/* It does... */
						janus_mutex_unlock(&recordings_mutex);
						JANUS_LOG(LOG_ERR, "Recording %"SCNu64" already exists!\n", id);
						error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_RECORDING_EXISTS;
						g_snprintf(error_cause, 512, "Recording %"SCNu64" already exists", id);
						goto error;
					}
				}
			}
			if(id == 0) {
				while(id == 0) {
					id = janus_random_uint64();
					if(g_hash_table_lookup(recordings, &id) != NULL) {
						/* Recording ID already taken, try another one */
						id = 0;
					}
				}
			}
			JANUS_LOG(LOG_VERB, "Starting new recording with ID %"SCNu64"\n", id);
			rec = g_malloc0(sizeof(janus_auviousrecordplay_recording));
			rec->id = id;
			rec->name = g_strdup(name_text);
			rec->viewers = NULL;
			rec->offer = NULL;
			g_atomic_int_set(&rec->destroyed, 0);
			g_atomic_int_set(&rec->completed, 0);
			janus_refcount_init(&rec->ref, janus_auviousrecordplay_recording_free);
			janus_refcount_increase(&rec->ref);	/* This is for the user writing the recording */
			janus_mutex_init(&rec->mutex);
			/* Check which codec we should record for audio and/or video */
			janus_sdp_find_preferred_codecs(offer, &rec->acodec, &rec->vcodec);
			/* We found preferred codecs: let's just make sure the direction is what we need */
			janus_sdp_mline *m = janus_sdp_mline_find(offer, JANUS_SDP_AUDIO);
			if(m != NULL && m->direction == JANUS_SDP_RECVONLY)
				rec->acodec = NULL;
			audio = (rec->acodec != NULL);
			if(audio) {
				JANUS_LOG(LOG_WARN, "Audio codec: %s\n", rec->acodec);
			}
			m = janus_sdp_mline_find(offer, JANUS_SDP_VIDEO);
			if(m != NULL && m->direction == JANUS_SDP_RECVONLY)
				rec->vcodec = NULL;
			video = (rec->vcodec != NULL);
			if(video) {
				JANUS_LOG(LOG_WARN, "Video codec: %s\n", rec->vcodec);
			}
			rec->audio_pt = AUDIO_PT;
			if(rec->acodec) {
				/* Some audio codecs have a fixed payload type that we can't mess with */
				if(!strcasecmp(rec->acodec, "pcmu"))
					rec->audio_pt = 0;
				else if(!strcasecmp(rec->acodec, "pcma"))
					rec->audio_pt = 8;
				else if(!strcasecmp(rec->acodec, "g722"))
					rec->audio_pt = 9;
			}
			rec->video_pt = VIDEO_PT;
			/* Create a date string */
			time_t t = time(NULL);
			struct tm *tmv = localtime(&t);
			char outstr[200];
			strftime(outstr, sizeof(outstr), "%Y-%m-%d %H:%M:%S", tmv);
			rec->date = g_strdup(outstr);
			if(audio) {
				char filename[256];
				if(filename_text != NULL) {
					g_snprintf(filename, 256, "%s-audio", filename_text);
				} else {
					g_snprintf(filename, 256, "rec-%"SCNu64"-audio", id);
				}
				rec->arc_file = g_strdup(filename);
				session->arc = janus_recorder_create(recordings_path, rec->acodec, rec->arc_file);
			}
			if(video) {
				char filename[256];
				if(filename_text != NULL) {
					g_snprintf(filename, 256, "%s-video", filename_text);
				} else {
					g_snprintf(filename, 256, "rec-%"SCNu64"-video", id);
				}
				rec->vrc_file = g_strdup(filename);
				session->vrc = janus_recorder_create(recordings_path, rec->vcodec, rec->vrc_file);
			}
			session->recorder = TRUE;
			session->recording = rec;
			session->sdp_version = 1;	/* This needs to be increased when it changes */
			session->sdp_sessid = janus_get_real_time();
			g_hash_table_insert(recordings, janus_uint64_dup(rec->id), rec);
			janus_mutex_unlock(&recordings_mutex);
			/* We need to prepare an answer */
recdone:
			answer = janus_sdp_generate_answer(offer,
				JANUS_SDP_OA_AUDIO, audio,
				JANUS_SDP_OA_AUDIO_CODEC, rec->acodec,
				JANUS_SDP_OA_AUDIO_DIRECTION, JANUS_SDP_RECVONLY,
				JANUS_SDP_OA_VIDEO, video,
				JANUS_SDP_OA_VIDEO_CODEC, rec->vcodec,
				JANUS_SDP_OA_VIDEO_DIRECTION, JANUS_SDP_RECVONLY,
				JANUS_SDP_OA_DATA, FALSE,
				JANUS_SDP_OA_DONE);
			g_free(answer->s_name);
			char s_name[100];
			g_snprintf(s_name, sizeof(s_name), "Recording %"SCNu64, rec->id);
			answer->s_name = g_strdup(s_name);
			/* Let's overwrite a couple o= fields, in case this is a renegotiation */
			answer->o_sessid = session->sdp_sessid;
			answer->o_version = session->sdp_version;
			/* Generate the SDP string */
			sdp = janus_sdp_write(answer);
			janus_sdp_destroy(offer);
			janus_sdp_destroy(answer);
			JANUS_LOG(LOG_VERB, "Going to answer this SDP:\n%s\n", sdp);
			/* If the user negotiated simulcasting, just stick with the base substream */
			json_t *msg_simulcast = json_object_get(msg->jsep, "simulcast");
			if(msg_simulcast) {
				JANUS_LOG(LOG_WARN, "Recording client negotiated simulcasting which we don't do here, falling back to base substream...\n");
				session->simulcast_ssrc = json_integer_value(json_object_get(msg_simulcast, "ssrc-0"));
			}
			/* Done! */
			result = json_object();
			json_object_set_new(result, "status", json_string("recording"));
			json_object_set_new(result, "id", json_integer(id));
			/* Also notify event handlers */
			if(!sdp_update && notify_events && gateway->events_is_enabled()) {
				json_t *info = json_object();
				json_object_set_new(info, "event", json_string("recording"));
				json_object_set_new(info, "id", json_integer(id));
				json_object_set_new(info, "audio", session->arc ? json_true() : json_false());
				json_object_set_new(info, "video", session->vrc ? json_true() : json_false());
				gateway->notify_event(&janus_auviousrecordplay_plugin, session->handle, info);
			}
		} else if(!strcasecmp(request_text, "play")) {
			if(msg_sdp) {
				JANUS_LOG(LOG_ERR, "A play request can't contain an SDP\n");
				error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_ELEMENT;
				g_snprintf(error_cause, 512, "A play request can't contain an SDP");
				goto error;
			}
			JANUS_LOG(LOG_VERB, "Replaying a recording\n");
			JANUS_VALIDATE_JSON_OBJECT(root, play_parameters,
				error_code, error_cause, TRUE,
				JANUS_AUVIOUSRECORDPLAY_ERROR_MISSING_ELEMENT, JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_ELEMENT);
			if(error_code != 0)
				goto error;
			json_t *restart = json_object_get(root, "restart");
			gboolean do_restart = restart ? json_is_true(restart) : FALSE;
			/* Check if this is a new playout, or if an update is taking place (i.e., ICE restart) */
			guint64 id_value = 0;
			janus_auviousrecordplay_recording *rec = NULL;
			const char *warning = NULL;
			if(sdp_update || do_restart) {
				/* Renegotiation: make sure the user provided an offer, and send answer */
				JANUS_LOG(LOG_VERB, "Request to perform an ICE restart on existing playout\n");
				if(session->recorder || session->recording == NULL || session->recording->offer == NULL) {
					JANUS_LOG(LOG_ERR, "Not a playout session, can't restart\n");
					error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_STATE;
					g_snprintf(error_cause, 512, "Not a playout session, can't restart");
					goto error;
				}
				rec = session->recording;
				id_value = rec->id;
				session->sdp_version++;		/* This needs to be increased when it changes */
				sdp_update = TRUE;
				/* Let's overwrite a couple o= fields, in case this is a renegotiation */
				char error_str[512];
				janus_sdp *offer = janus_sdp_parse(rec->offer, error_str, sizeof(error_str));
				if(offer == NULL) {
					JANUS_LOG(LOG_ERR, "Invalid offer, can't restart\n");
					error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_STATE;
					g_snprintf(error_cause, 512, "Invalid, can't restart");
					goto error;
				}
				offer->o_sessid = session->sdp_sessid;
				offer->o_version = session->sdp_version;
				sdp = janus_sdp_write(offer);
				janus_sdp_destroy(offer);
				goto playdone;
			}
			/* If we got here, it's a new playout */
			json_t *id = json_object_get(root, "id");
			id_value = json_integer_value(id);
			/* Look for this recording */
			janus_mutex_lock(&recordings_mutex);
			rec = g_hash_table_lookup(recordings, &id_value);
			if(rec != NULL)
				janus_refcount_increase(&rec->ref);
			janus_mutex_unlock(&recordings_mutex);
			if(rec == NULL || rec->offer == NULL || g_atomic_int_get(&rec->destroyed)) {
				if(rec != NULL)
					janus_refcount_decrease(&rec->ref);
				JANUS_LOG(LOG_ERR, "No such recording\n");
				error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_NOT_FOUND;
				g_snprintf(error_cause, 512, "No such recording");
				goto error;
			}
			/* Access the frames */
			if(rec->arc_file) {
				session->aframes = janus_auviousrecordplay_get_frames(recordings_path, rec->arc_file);
				if(session->aframes == NULL) {
					JANUS_LOG(LOG_WARN, "Error opening audio recording, trying to go on anyway\n");
					warning = "Broken audio file, playing video only";
				}
			}
			if(rec->vrc_file) {
				session->vframes = janus_auviousrecordplay_get_frames(recordings_path, rec->vrc_file);
				if(session->vframes == NULL) {
					JANUS_LOG(LOG_WARN, "Error opening video recording, trying to go on anyway\n");
					warning = "Broken video file, playing audio only";
				}
			}
			if(session->aframes == NULL && session->vframes == NULL) {
				error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_RECORDING;
				g_snprintf(error_cause, 512, "Error opening recording files");
				goto error;
			}
			session->recording = rec;
			session->recorder = FALSE;
			rec->viewers = g_list_append(rec->viewers, session);
			/* Send this viewer the prepared offer  */
			sdp = g_strdup(rec->offer);
playdone:
			JANUS_LOG(LOG_VERB, "Going to offer this SDP:\n%s\n", sdp);
			/* Done! */
			result = json_object();
			json_object_set_new(result, "status", json_string(sdp_update ? "restarting" : "preparing"));
			json_object_set_new(result, "id", json_integer(id_value));
			if(warning)
				json_object_set_new(result, "warning", json_string(warning));
			/* Also notify event handlers */
			if(!sdp_update && notify_events && gateway->events_is_enabled()) {
				json_t *info = json_object();
				json_object_set_new(info, "event", json_string("playout"));
				json_object_set_new(info, "id", json_integer(id_value));
				json_object_set_new(info, "audio", session->aframes ? json_true() : json_false());
				json_object_set_new(info, "video", session->vframes ? json_true() : json_false());
				gateway->notify_event(&janus_auviousrecordplay_plugin, session->handle, info);
			}
		} else if(!strcasecmp(request_text, "start")) {
			if(!session->aframes && !session->vframes) {
				JANUS_LOG(LOG_ERR, "Not a playout session, can't start\n");
				error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_STATE;
				g_snprintf(error_cause, 512, "Not a playout session, can't start");
				goto error;
			}
			/* Just a final message we make use of, e.g., to receive an ANSWER to our OFFER for a playout */
			if(!msg_sdp) {
				JANUS_LOG(LOG_ERR, "Missing SDP answer\n");
				error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_MISSING_ELEMENT;
				g_snprintf(error_cause, 512, "Missing SDP answer");
				goto error;
			}
			/* Done! */
			result = json_object();
			json_object_set_new(result, "status", json_string("playing"));
			/* Also notify event handlers */
			if(notify_events && gateway->events_is_enabled()) {
				json_t *info = json_object();
				json_object_set_new(info, "event", json_string("playing"));
				json_object_set_new(info, "id", json_integer(session->recording->id));
				gateway->notify_event(&janus_auviousrecordplay_plugin, session->handle, info);
			}
		} else if(!strcasecmp(request_text, "stop")) {
			/* Done! */
			result = json_object();
			json_object_set_new(result, "status", json_string("stopped"));
			if(session->recording) {
				json_object_set_new(result, "id", json_integer(session->recording->id));
				/* Also notify event handlers */
				if(notify_events && gateway->events_is_enabled()) {
					json_t *info = json_object();
					json_object_set_new(info, "event", json_string("stopped"));
					if(session->recording)
						json_object_set_new(info, "id", json_integer(session->recording->id));
					gateway->notify_event(&janus_auviousrecordplay_plugin, session->handle, info);
				}
			}
			janus_auviousrecordplay_hangup_media(session->handle);
		} else if (!strcasecmp(request_text, "record-generate-offer")) {
			JANUS_LOG(LOG_VERB, "Generating an offer for recording\n");
			char s_name[100];
			guint32 id = g_random_int();
			g_snprintf(s_name, sizeof(s_name), "Recording %" SCNu32, id);

			gboolean offer_audio = TRUE;
			gboolean offer_video = TRUE;

			const char *acodec = NULL;
			const char *vcodec = NULL;

			if (json_object_get(msg->message, "audio") != NULL) {
				offer_audio = json_is_true(json_object_get(msg->message, "audio"));
				if (json_object_get(msg->message, "acodec") != NULL) {
					acodec = json_string_value(json_object_get(msg->message, "acodec"));
				} else {
					acodec = "opus";
				}
			}

			if (json_object_get(msg->message, "video") != NULL) {
				offer_video = json_is_true(json_object_get(msg->message, "video"));
				if (json_object_get(msg->message, "vcodec") != NULL) {
					vcodec = json_string_value(json_object_get(msg->message, "vcodec"));
				} else {
					vcodec = "h264";
				}
			}

			janus_sdp *offer = janus_sdp_generate_offer(
				s_name, "1.1.1.1",
				JANUS_SDP_OA_AUDIO, offer_audio,
				JANUS_SDP_OA_AUDIO_CODEC, acodec,
				JANUS_SDP_OA_AUDIO_PT, AUDIO_PT,
				JANUS_SDP_OA_AUDIO_DIRECTION, JANUS_SDP_RECVONLY,
				JANUS_SDP_OA_VIDEO, offer_video,
				JANUS_SDP_OA_VIDEO_CODEC, vcodec,
				JANUS_SDP_OA_VIDEO_PT, VIDEO_PT,
				JANUS_SDP_OA_VIDEO_DIRECTION, JANUS_SDP_RECVONLY,
				JANUS_SDP_OA_DATA, FALSE,
				JANUS_SDP_OA_DONE);

			sdp = janus_sdp_write(offer);
			janus_sdp_destroy(offer);
		} else if(!strcasecmp(request_text, "record-process-answer")) {
			if(!msg_sdp || !msg_sdp_type || strcasecmp(msg_sdp_type, "answer")) {
				JANUS_LOG(LOG_ERR, "Missing SDP answer\n");
				error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_MISSING_ELEMENT;
				g_snprintf(error_cause, 512, "Missing SDP answer");
				goto error;
			}
			JANUS_VALIDATE_JSON_OBJECT(root, record_parameters,
				error_code, error_cause, TRUE,
				JANUS_AUVIOUSRECORDPLAY_ERROR_MISSING_ELEMENT, JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_ELEMENT);
			if(error_code != 0)
				goto error;
			char error_str[512];
			janus_sdp *answer = janus_sdp_parse(msg_sdp, error_str, sizeof(error_str));
			if(answer == NULL) {
				json_decref(event);
				JANUS_LOG(LOG_ERR, "Error parsing answer: %s\n", error_str);
				error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_SDP;
				g_snprintf(error_cause, 512, "Error parsing answer: %s", error_str);
				goto error;
			}
			json_t *name = json_object_get(root, "name");
			const char *name_text = json_string_value(name);
			json_t *filename = json_object_get(root, "filename");
			if(filename) {
				filename_text = json_string_value(filename);
			}
			json_t *update = json_object_get(root, "update");
			gboolean do_update = update ? json_is_true(update) : FALSE;
			if(do_update && !sdp_update) {
				JANUS_LOG(LOG_WARN, "Got a 'update' request, but no SDP update? Ignoring...\n");
			}
			/* Check if this is a new recorder, or if an update is taking place (i.e., ICE restart) */
			guint64 id = 0;
			janus_auviousrecordplay_recording *rec = NULL;
			gboolean audio = FALSE, video = FALSE;
			if(sdp_update) {
				/* Renegotiation: make sure the user provided an offer, and send answer */
				JANUS_LOG(LOG_VERB, "Request to update existing recorder\n");
				if(!session->recorder || !session->recording) {
					JANUS_LOG(LOG_ERR, "Not a recording session, can't update\n");
					error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_STATE;
					g_snprintf(error_cause, 512, "Not a recording session, can't update");
					goto error;
				}
				id = session->recording->id;
				rec = session->recording;
				session->sdp_version++;		/* This needs to be increased when it changes */
				audio = (session->arc != NULL);
				video = (session->vrc != NULL);
				sdp_update = do_update;
				goto recadone;
			}
			/* If we're here, we're doing a new recording */
			janus_mutex_lock(&recordings_mutex);
			json_t *rec_id = json_object_get(root, "id");
			if(rec_id) {
				id = json_integer_value(rec_id);
				if(id > 0) {
					/* Let's make sure the ID doesn't exist already */
					if(g_hash_table_lookup(recordings, &id) != NULL) {
						/* It does... */
						janus_mutex_unlock(&recordings_mutex);
						JANUS_LOG(LOG_ERR, "Recording %"SCNu64" already exists!\n", id);
						error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_RECORDING_EXISTS;
						g_snprintf(error_cause, 512, "Recording %"SCNu64" already exists", id);
						goto error;
					}
				}
			}
			if(id == 0) {
				while(id == 0) {
					id = janus_random_uint64();
					if(g_hash_table_lookup(recordings, &id) != NULL) {
						/* Recording ID already taken, try another one */
						id = 0;
					}
				}
			}
			JANUS_LOG(LOG_VERB, "Starting new recording with ID %"SCNu64"\n", id);
			rec = g_malloc0(sizeof(janus_auviousrecordplay_recording));
			rec->id = id;
			rec->name = g_strdup(name_text);
			rec->viewers = NULL;
			rec->offer = NULL;
			g_atomic_int_set(&rec->destroyed, 0);
			g_atomic_int_set(&rec->completed, 0);
			janus_refcount_init(&rec->ref, janus_auviousrecordplay_recording_free);
			janus_refcount_increase(&rec->ref);	/* This is for the user writing the recording */
			janus_mutex_init(&rec->mutex);
			/* Check which codec we should record for audio and/or video */
			janus_sdp_find_preferred_codecs(answer, &rec->acodec, &rec->vcodec);
			/* We found preferred codecs: let's just make sure the direction is what we need */
			janus_sdp_mline *m = janus_sdp_mline_find(answer, JANUS_SDP_AUDIO);
			if(m != NULL && m->direction == JANUS_SDP_RECVONLY)
				rec->acodec = NULL;
			audio = (rec->acodec != NULL);
			if(audio) {
				JANUS_LOG(LOG_WARN, "Audio codec: %s\n", rec->acodec);
			}
			m = janus_sdp_mline_find(answer, JANUS_SDP_VIDEO);
			if(m != NULL && m->direction == JANUS_SDP_RECVONLY)
				rec->vcodec = NULL;
			video = (rec->vcodec != NULL);
			if(video) {
				JANUS_LOG(LOG_WARN, "Video codec: %s\n", rec->vcodec);
			}
			rec->audio_pt = AUDIO_PT;
			if(rec->acodec) {
				/* Some audio codecs have a fixed payload type that we can't mess with */
				if(!strcasecmp(rec->acodec, "pcmu"))
					rec->audio_pt = 0;
				else if(!strcasecmp(rec->acodec, "pcma"))
					rec->audio_pt = 8;
				else if(!strcasecmp(rec->acodec, "g722"))
					rec->audio_pt = 9;
			}
			rec->video_pt = VIDEO_PT;
			/* Create a date string */
			time_t t = time(NULL);
			struct tm *tmv = localtime(&t);
			char outstr[200];
			strftime(outstr, sizeof(outstr), "%Y-%m-%d %H:%M:%S", tmv);
			rec->date = g_strdup(outstr);
			if(audio) {
				char filename[256];
				if(filename_text != NULL) {
					g_snprintf(filename, 256, "%s-audio", filename_text);
				} else {
					g_snprintf(filename, 256, "rec-%"SCNu64"-audio", id);
				}
				rec->arc_file = g_strdup(filename);
				session->arc = janus_recorder_create(recordings_path, rec->acodec, rec->arc_file);
			}
			if(video) {
				char filename[256];
				if(filename_text != NULL) {
					g_snprintf(filename, 256, "%s-video", filename_text);
				} else {
					g_snprintf(filename, 256, "rec-%"SCNu64"-video", id);
				}
				rec->vrc_file = g_strdup(filename);
				session->vrc = janus_recorder_create(recordings_path, rec->vcodec, rec->vrc_file);
			}
			session->recorder = TRUE;
			session->recording = rec;
			session->sdp_version = 1;	/* This needs to be increased when it changes */
			session->sdp_sessid = janus_get_real_time();
			g_hash_table_insert(recordings, janus_uint64_dup(rec->id), rec);
			janus_mutex_unlock(&recordings_mutex);
			/* We need to prepare an answer */
recadone:
			if (sdp_update) {
				janus_sdp * offer = answer;
				answer = janus_sdp_generate_answer(offer,
					JANUS_SDP_OA_AUDIO, audio,
					JANUS_SDP_OA_AUDIO_CODEC, rec->acodec,
					JANUS_SDP_OA_AUDIO_DIRECTION, JANUS_SDP_RECVONLY,
					JANUS_SDP_OA_VIDEO, video,
					JANUS_SDP_OA_VIDEO_CODEC, rec->vcodec,
					JANUS_SDP_OA_VIDEO_DIRECTION, JANUS_SDP_RECVONLY,
					JANUS_SDP_OA_DATA, FALSE,
					JANUS_SDP_OA_DONE);
				g_free(answer->s_name);
				char s_name[100];
				g_snprintf(s_name, sizeof(s_name), "Recording %"SCNu64, rec->id);
				answer->s_name = g_strdup(s_name);
				/* Let's overwrite a couple o= fields, in case this is a renegotiation */
				answer->o_sessid = session->sdp_sessid;
				answer->o_version = session->sdp_version;
				/* Generate the SDP string */
				sdp = janus_sdp_write(answer);
				janus_sdp_destroy(offer);
				janus_sdp_destroy(answer);
				JANUS_LOG(LOG_VERB, "Going to answer this SDP:\n%s\n", sdp);
			} else {
				janus_sdp_destroy(answer);
			}
			/* If the user negotiated simulcasting, just stick with the base substream */
			json_t *msg_simulcast = json_object_get(msg->jsep, "simulcast");
			if(msg_simulcast) {
				JANUS_LOG(LOG_WARN, "Recording client negotiated simulcasting which we don't do here, falling back to base substream...\n");
				session->simulcast_ssrc = json_integer_value(json_object_get(msg_simulcast, "ssrc-0"));
			}
			/* Done! */
			result = json_object();
			json_object_set_new(result, "status", json_string("recording"));
			json_object_set_new(result, "id", json_integer(id));
			/* Also notify event handlers */
			if(!sdp_update && notify_events && gateway->events_is_enabled()) {
				json_t *info = json_object();
				json_object_set_new(info, "event", json_string("recording"));
				json_object_set_new(info, "id", json_integer(id));
				json_object_set_new(info, "audio", session->arc ? json_true() : json_false());
				json_object_set_new(info, "video", session->vrc ? json_true() : json_false());
				gateway->notify_event(&janus_auviousrecordplay_plugin, session->handle, info);
			}
		} else {
			JANUS_LOG(LOG_ERR, "Unknown request '%s'\n", request_text);
			error_code = JANUS_AUVIOUSRECORDPLAY_ERROR_INVALID_REQUEST;
			g_snprintf(error_cause, 512, "Unknown request '%s'", request_text);
			goto error;
		}

		/* Prepare JSON event */
		event = json_object();
		json_object_set_new(event, "recordplay", json_string("event"));
		if(result != NULL)
			json_object_set_new(event, "result", result);
		if(!sdp) {
			int ret = gateway->push_event(msg->handle, &janus_auviousrecordplay_plugin, msg->transaction, event, NULL);
			JANUS_LOG(LOG_VERB, "  >> Pushing event: %d (%s)\n", ret, janus_get_api_error(ret));
			json_decref(event);
		} else {
			const char *type = session->recorder ? "answer" : "offer";
			json_t *jsep = json_pack("{ssss}", "type", type, "sdp", sdp);
			if(sdp_update)
				json_object_set_new(jsep, "restart", json_true());
			/* How long will the gateway take to push the event? */
			g_atomic_int_set(&session->hangingup, 0);
			gint64 start = janus_get_monotonic_time();
			int res = gateway->push_event(msg->handle, &janus_auviousrecordplay_plugin, msg->transaction, event, jsep);
			JANUS_LOG(LOG_VERB, "  >> Pushing event: %d (took %"SCNu64" us)\n",
				res, janus_get_monotonic_time()-start);
			g_free(sdp);
			json_decref(event);
			json_decref(jsep);
		}
		janus_auviousrecordplay_message_free(msg);
		JANUS_LOG(LOG_VERB, "  >> Listed recordings: %u\n", g_hash_table_size(recordings));
		continue;
error:
		{
			/* Prepare JSON error event */
			json_t *event = json_object();
			json_object_set_new(event, "recordplay", json_string("event"));
			json_object_set_new(event, "error_code", json_integer(error_code));
			json_object_set_new(event, "error", json_string(error_cause));
			int ret = gateway->push_event(msg->handle, &janus_auviousrecordplay_plugin, msg->transaction, event, NULL);
			JANUS_LOG(LOG_VERB, "  >> Pushing event: %d (%s)\n", ret, janus_get_api_error(ret));
			json_decref(event);
			janus_auviousrecordplay_message_free(msg);
		}
	}
	JANUS_LOG(LOG_VERB, "LeavingRecord&Play handler thread\n");
	return NULL;
}

janus_auviousrecordplay_frame_packet *janus_auviousrecordplay_get_frames(const char *dir, const char *filename) {
	if(!dir || !filename)
		return NULL;
	/* Open the file */
	char source[1024];
	if(strstr(filename, ".mjr"))
		g_snprintf(source, 1024, "%s/%s", dir, filename);
	else
		g_snprintf(source, 1024, "%s/%s.mjr", dir, filename);
	FILE *file = fopen(source, "rb");
	if(file == NULL) {
		JANUS_LOG(LOG_ERR, "Could not open file %s\n", source);
		return NULL;
	}
	fseek(file, 0L, SEEK_END);
	long fsize = ftell(file);
	fseek(file, 0L, SEEK_SET);
	JANUS_LOG(LOG_VERB, "File is %zu bytes\n", fsize);

	/* Pre-parse */
	JANUS_LOG(LOG_VERB, "Pre-parsing file %s to generate ordered index...\n", source);
	gboolean parsed_header = FALSE;
	int bytes = 0;
	long offset = 0;
	uint16_t len = 0, count = 0;
	uint32_t first_ts = 0, last_ts = 0, reset = 0;	/* To handle whether there's a timestamp reset in the recording */
	char prebuffer[1500];
	memset(prebuffer, 0, 1500);
	/* Let's look for timestamp resets first */
	while(offset < fsize) {
		/* Read frame header */
		fseek(file, offset, SEEK_SET);
		bytes = fread(prebuffer, sizeof(char), 8, file);
		if(bytes != 8 || prebuffer[0] != 'M') {
			JANUS_LOG(LOG_ERR, "Invalid header...\n");
			fclose(file);
			return NULL;
		}
		if(prebuffer[1] == 'E') {
			/* Either the old .mjr format header ('MEETECHO' header followed by 'audio' or 'video'), or a frame */
			offset += 8;
			bytes = fread(&len, sizeof(uint16_t), 1, file);
			len = ntohs(len);
			offset += 2;
			if(len == 5 && !parsed_header) {
				/* This is the main header */
				parsed_header = TRUE;
				JANUS_LOG(LOG_VERB, "Old .mjr header format\n");
				bytes = fread(prebuffer, sizeof(char), 5, file);
				if(prebuffer[0] == 'v') {
					JANUS_LOG(LOG_INFO, "This is an old video recording, assuming VP8\n");
				} else if(prebuffer[0] == 'a') {
					JANUS_LOG(LOG_INFO, "This is an old audio recording, assuming Opus\n");
				} else {
					JANUS_LOG(LOG_WARN, "Unsupported recording media type...\n");
					fclose(file);
					return NULL;
				}
				offset += len;
				continue;
			} else if(len < 12) {
				/* Not RTP, skip */
				JANUS_LOG(LOG_VERB, "Skipping packet (not RTP?)\n");
				offset += len;
				continue;
			}
		} else if(prebuffer[1] == 'J') {
			/* New .mjr format, the header may contain useful info */
			offset += 8;
			bytes = fread(&len, sizeof(uint16_t), 1, file);
			len = ntohs(len);
			offset += 2;
			if(len > 0 && !parsed_header) {
				/* This is the info header */
				JANUS_LOG(LOG_VERB, "New .mjr header format\n");
				bytes = fread(prebuffer, sizeof(char), len, file);
				if(bytes < 0) {
					JANUS_LOG(LOG_ERR, "Error reading from file... %s\n", strerror(errno));
					fclose(file);
					return NULL;
				}
				parsed_header = TRUE;
				prebuffer[len] = '\0';
				json_error_t error;
				json_t *info = json_loads(prebuffer, 0, &error);
				if(!info) {
					JANUS_LOG(LOG_ERR, "JSON error: on line %d: %s\n", error.line, error.text);
					JANUS_LOG(LOG_WARN, "Error parsing info header...\n");
					fclose(file);
					return NULL;
				}
				/* Is it audio or video? */
				json_t *type = json_object_get(info, "t");
				if(!type || !json_is_string(type)) {
					JANUS_LOG(LOG_WARN, "Missing/invalid recording type in info header...\n");
					json_decref(info);
					fclose(file);
					return NULL;
				}
				const char *t = json_string_value(type);
				int video = 0;
				gint64 c_time = 0, w_time = 0;
				if(!strcasecmp(t, "v")) {
					video = 1;
				} else if(!strcasecmp(t, "a")) {
					video = 0;
				} else {
					JANUS_LOG(LOG_WARN, "Unsupported recording type '%s' in info header...\n", t);
					json_decref(info);
					fclose(file);
					return NULL;
				}
				/* What codec was used? */
				json_t *codec = json_object_get(info, "c");
				if(!codec || !json_is_string(codec)) {
					JANUS_LOG(LOG_WARN, "Missing recording codec in info header...\n");
					json_decref(info);
					fclose(file);
					return NULL;
				}
				const char *c = json_string_value(codec);
				/* When was the file created? */
				json_t *created = json_object_get(info, "s");
				if(!created || !json_is_integer(created)) {
					JANUS_LOG(LOG_WARN, "Missing recording created time in info header...\n");
					json_decref(info);
					fclose(file);
					return NULL;
				}
				c_time = json_integer_value(created);
				/* When was the first frame written? */
				json_t *written = json_object_get(info, "u");
				if(!written || !json_is_integer(written)) {
					JANUS_LOG(LOG_WARN, "Missing recording written time in info header...\n");
					json_decref(info);
					fclose(file);
					return NULL;
				}
				w_time = json_integer_value(created);
				/* Summary */
				JANUS_LOG(LOG_VERB, "This is %s recording:\n", video ? "a video" : "an audio");
				JANUS_LOG(LOG_VERB, "  -- Codec:   %s\n", c);
				JANUS_LOG(LOG_VERB, "  -- Created: %"SCNi64"\n", c_time);
				JANUS_LOG(LOG_VERB, "  -- Written: %"SCNi64"\n", w_time);
				json_decref(info);
			}
		} else {
			JANUS_LOG(LOG_ERR, "Invalid header...\n");
			fclose(file);
			return NULL;
		}
		/* Only read RTP header */
		bytes = fread(prebuffer, sizeof(char), 16, file);
		janus_rtp_header *rtp = (janus_rtp_header *)prebuffer;
		if(last_ts == 0) {
			first_ts = ntohl(rtp->timestamp);
			if(first_ts > 1000*1000)	/* Just used to check whether a packet is pre- or post-reset */
				first_ts -= 1000*1000;
		} else {
			if(ntohl(rtp->timestamp) < last_ts) {
				/* The new timestamp is smaller than the next one, is it a timestamp reset or simply out of order? */
				if(last_ts-ntohl(rtp->timestamp) > 2*1000*1000*1000) {
					reset = ntohl(rtp->timestamp);
					JANUS_LOG(LOG_VERB, "Timestamp reset: %"SCNu32"\n", reset);
				}
			} else if(ntohl(rtp->timestamp) < reset) {
				JANUS_LOG(LOG_VERB, "Updating timestamp reset: %"SCNu32" (was %"SCNu32")\n", ntohl(rtp->timestamp), reset);
				reset = ntohl(rtp->timestamp);
			}
		}
		last_ts = ntohl(rtp->timestamp);
		/* Skip data for now */
		offset += len;
	}
	/* Now let's parse the frames and order them */
	offset = 0;
	janus_auviousrecordplay_frame_packet *list = NULL, *last = NULL;
	while(offset < fsize) {
		/* Read frame header */
		fseek(file, offset, SEEK_SET);
		bytes = fread(prebuffer, sizeof(char), 8, file);
		prebuffer[8] = '\0';
		JANUS_LOG(LOG_HUGE, "Header: %s\n", prebuffer);
		offset += 8;
		bytes = fread(&len, sizeof(uint16_t), 1, file);
		len = ntohs(len);
		JANUS_LOG(LOG_HUGE, "  -- Length: %"SCNu16"\n", len);
		offset += 2;
		if(prebuffer[1] == 'J' || len < 12) {
			/* Not RTP, skip */
			JANUS_LOG(LOG_HUGE, "  -- Not RTP, skipping\n");
			offset += len;
			continue;
		}
		/* Only read RTP header */
		bytes = fread(prebuffer, sizeof(char), 16, file);
		if(bytes < 0) {
			JANUS_LOG(LOG_WARN, "Error reading RTP header, stopping here...\n");
			break;
		}
		janus_rtp_header *rtp = (janus_rtp_header *)prebuffer;
		JANUS_LOG(LOG_HUGE, "  -- RTP packet (ssrc=%"SCNu32", pt=%"SCNu16", ext=%"SCNu16", seq=%"SCNu16", ts=%"SCNu32")\n",
				ntohl(rtp->ssrc), rtp->type, rtp->extension, ntohs(rtp->seq_number), ntohl(rtp->timestamp));
		/* Generate frame packet and insert in the ordered list */
		janus_auviousrecordplay_frame_packet *p = g_malloc(sizeof(janus_auviousrecordplay_frame_packet));
		p->seq = ntohs(rtp->seq_number);
		if(reset == 0) {
			/* Simple enough... */
			p->ts = ntohl(rtp->timestamp);
		} else {
			/* Is this packet pre- or post-reset? */
			if(ntohl(rtp->timestamp) > first_ts) {
				/* Pre-reset... */
				p->ts = ntohl(rtp->timestamp);
			} else {
				/* Post-reset... */
				uint64_t max32 = UINT32_MAX;
				max32++;
				p->ts = max32+ntohl(rtp->timestamp);
			}
		}
		p->len = len;
		p->offset = offset;
		p->next = NULL;
		p->prev = NULL;
		if(list == NULL) {
			/* First element becomes the list itself (and the last item), at least for now */
			list = p;
			last = p;
		} else {
			/* Check where we should insert this, starting from the end */
			int added = 0;
			janus_auviousrecordplay_frame_packet *tmp = last;
			while(tmp) {
				if(tmp->ts < p->ts) {
					/* The new timestamp is greater than the last one we have, append */
					added = 1;
					if(tmp->next != NULL) {
						/* We're inserting */
						tmp->next->prev = p;
						p->next = tmp->next;
					} else {
						/* Update the last packet */
						last = p;
					}
					tmp->next = p;
					p->prev = tmp;
					break;
				} else if(tmp->ts == p->ts) {
					/* Same timestamp, check the sequence number */
					if(tmp->seq < p->seq && (abs(tmp->seq - p->seq) < 10000)) {
						/* The new sequence number is greater than the last one we have, append */
						added = 1;
						if(tmp->next != NULL) {
							/* We're inserting */
							tmp->next->prev = p;
							p->next = tmp->next;
						} else {
							/* Update the last packet */
							last = p;
						}
						tmp->next = p;
						p->prev = tmp;
						break;
					} else if(tmp->seq > p->seq && (abs(tmp->seq - p->seq) > 10000)) {
						/* The new sequence number (resetted) is greater than the last one we have, append */
						added = 1;
						if(tmp->next != NULL) {
							/* We're inserting */
							tmp->next->prev = p;
							p->next = tmp->next;
						} else {
							/* Update the last packet */
							last = p;
						}
						tmp->next = p;
						p->prev = tmp;
						break;
					}
				}
				/* If either the timestamp ot the sequence number we just got is smaller, keep going back */
				tmp = tmp->prev;
			}
			if(!added) {
				/* We reached the start */
				p->next = list;
				list->prev = p;
				list = p;
			}
		}
		/* Skip data for now */
		offset += len;
		count++;
	}

	JANUS_LOG(LOG_VERB, "Counted %"SCNu16" RTP packets\n", count);
	janus_auviousrecordplay_frame_packet *tmp = list;
	count = 0;
	while(tmp) {
		count++;
		JANUS_LOG(LOG_HUGE, "[%10lu][%4d] seq=%"SCNu16", ts=%"SCNu64"\n", tmp->offset, tmp->len, tmp->seq, tmp->ts);
		tmp = tmp->next;
	}
	JANUS_LOG(LOG_VERB, "Counted %"SCNu16" frame packets\n", count);

	/* Done! */
	fclose(file);
	return list;
}

static void *janus_auviousrecordplay_playout_thread(void *data) {
	janus_auviousrecordplay_session *session = (janus_auviousrecordplay_session *)data;
	if(!session) {
		JANUS_LOG(LOG_ERR, "Invalid session, can't start playout thread...\n");
		g_thread_unref(g_thread_self());
		return NULL;
	}
	if(!session->recording) {
		janus_refcount_decrease(&session->ref);
		JANUS_LOG(LOG_ERR, "No recording object, can't start playout thread...\n");
		g_thread_unref(g_thread_self());
		return NULL;
	}
	janus_refcount_increase(&session->recording->ref);
	janus_auviousrecordplay_recording *rec = session->recording;
	if(session->recorder) {
		janus_refcount_decrease(&rec->ref);
		janus_refcount_decrease(&session->ref);
		JANUS_LOG(LOG_ERR, "This is a recorder, can't start playout thread...\n");
		g_thread_unref(g_thread_self());
		return NULL;
	}
	if(!session->aframes && !session->vframes) {
		janus_refcount_decrease(&rec->ref);
		janus_refcount_decrease(&session->ref);
		JANUS_LOG(LOG_ERR, "No audio and no video frames, can't start playout thread...\n");
		g_thread_unref(g_thread_self());
		return NULL;
	}
	JANUS_LOG(LOG_INFO, "Joining playout thread\n");
	/* Open the files */
	FILE *afile = NULL, *vfile = NULL;
	if(session->aframes) {
		char source[1024];
		if(strstr(rec->arc_file, ".mjr"))
			g_snprintf(source, 1024, "%s/%s", recordings_path, rec->arc_file);
		else
			g_snprintf(source, 1024, "%s/%s.mjr", recordings_path, rec->arc_file);
		afile = fopen(source, "rb");
		if(afile == NULL) {
			janus_refcount_decrease(&rec->ref);
			janus_refcount_decrease(&session->ref);
			JANUS_LOG(LOG_ERR, "Could not open audio file %s, can't start playout thread...\n", source);
			g_thread_unref(g_thread_self());
			return NULL;
		}
	}
	if(session->vframes) {
		char source[1024];
		if(strstr(rec->vrc_file, ".mjr"))
			g_snprintf(source, 1024, "%s/%s", recordings_path, rec->vrc_file);
		else
			g_snprintf(source, 1024, "%s/%s.mjr", recordings_path, rec->vrc_file);
		vfile = fopen(source, "rb");
		if(vfile == NULL) {
			janus_refcount_decrease(&rec->ref);
			janus_refcount_decrease(&session->ref);
			JANUS_LOG(LOG_ERR, "Could not open video file %s, can't start playout thread...\n", source);
			if(afile)
				fclose(afile);
			afile = NULL;
			g_thread_unref(g_thread_self());
			return NULL;
		}
	}

	/* Timer */
	gboolean asent = FALSE, vsent = FALSE;
	struct timeval now, abefore, vbefore;
	time_t d_s, d_us;
	gettimeofday(&now, NULL);
	gettimeofday(&abefore, NULL);
	gettimeofday(&vbefore, NULL);

	janus_auviousrecordplay_frame_packet *audio = session->aframes, *video = session->vframes;
	char *buffer = g_malloc0(1500);
	int bytes = 0;
	int64_t ts_diff = 0, passed = 0;

	int audio_pt = session->recording->audio_pt;
	int video_pt = session->recording->video_pt;

	int akhz = 48;
	if(audio_pt == 0 || audio_pt == 8 || audio_pt == 9)
		akhz = 8;
	int vkhz = 90;

	while(!g_atomic_int_get(&session->destroyed) && session->active
			&& !g_atomic_int_get(&rec->destroyed) && (audio || video)) {
		if(!asent && !vsent) {
			/* We skipped the last round, so sleep a bit (5ms) */
			g_usleep(5000);
		}
		asent = FALSE;
		vsent = FALSE;
		if(audio) {
			if(audio == session->aframes) {
				/* First packet, send now */
				fseek(afile, audio->offset, SEEK_SET);
				bytes = fread(buffer, sizeof(char), audio->len, afile);
				if(bytes != audio->len)
					JANUS_LOG(LOG_WARN, "Didn't manage to read all the bytes we needed (%d < %d)...\n", bytes, audio->len);
				/* Update payload type */
				janus_rtp_header *rtp = (janus_rtp_header *)buffer;
				rtp->type = audio_pt;
				if(gateway != NULL) {
          janus_plugin_rtp prtp = { .video = FALSE, .buffer = (char *)buffer, .length = bytes };
          janus_plugin_rtp_extensions_reset(&prtp.extensions);
					gateway->relay_rtp(session->handle, &prtp); 
        }
				gettimeofday(&now, NULL);
				abefore.tv_sec = now.tv_sec;
				abefore.tv_usec = now.tv_usec;
				asent = TRUE;
				audio = audio->next;
			} else {
				/* What's the timestamp skip from the previous packet? */
				ts_diff = audio->ts - audio->prev->ts;
				ts_diff = (ts_diff*1000)/akhz;
				/* Check if it's time to send */
				gettimeofday(&now, NULL);
				d_s = now.tv_sec - abefore.tv_sec;
				d_us = now.tv_usec - abefore.tv_usec;
				if(d_us < 0) {
					d_us += 1000000;
					--d_s;
				}
				passed = d_s*1000000 + d_us;
				if(passed < (ts_diff-5000)) {
					asent = FALSE;
				} else {
					/* Update the reference time */
					abefore.tv_usec += ts_diff%1000000;
					if(abefore.tv_usec > 1000000) {
						abefore.tv_sec++;
						abefore.tv_usec -= 1000000;
					}
					if(ts_diff/1000000 > 0) {
						abefore.tv_sec += ts_diff/1000000;
						abefore.tv_usec -= ts_diff/1000000;
					}
					/* Send now */
					fseek(afile, audio->offset, SEEK_SET);
					bytes = fread(buffer, sizeof(char), audio->len, afile);
					if(bytes != audio->len)
						JANUS_LOG(LOG_WARN, "Didn't manage to read all the bytes we needed (%d < %d)...\n", bytes, audio->len);
					/* Update payload type */
					janus_rtp_header *rtp = (janus_rtp_header *)buffer;
					rtp->type = audio_pt;
					if(gateway != NULL) {
            janus_plugin_rtp prtp = { .video = FALSE, .buffer = (char *)buffer, .length = bytes };
            janus_plugin_rtp_extensions_reset(&prtp.extensions);
            gateway->relay_rtp(session->handle, &prtp);
          }
					asent = TRUE;
					audio = audio->next;
				}
			}
		}
		if(video) {
			if(video == session->vframes) {
				/* First packets: there may be many of them with the same timestamp, send them all */
				uint64_t ts = video->ts;
				while(video && video->ts == ts) {
					fseek(vfile, video->offset, SEEK_SET);
					bytes = fread(buffer, sizeof(char), video->len, vfile);
					if(bytes != video->len)
						JANUS_LOG(LOG_WARN, "Didn't manage to read all the bytes we needed (%d < %d)...\n", bytes, video->len);
					/* Update payload type */
					janus_rtp_header *rtp = (janus_rtp_header *)buffer;
					rtp->type = video_pt;
					if(gateway != NULL) {
            janus_plugin_rtp prtp = { .video = TRUE, .buffer = (char *)buffer, .length = bytes };
            janus_plugin_rtp_extensions_reset(&prtp.extensions);
            gateway->relay_rtp(session->handle, &prtp);
          }
					video = video->next;
				}
				vsent = TRUE;
				gettimeofday(&now, NULL);
				vbefore.tv_sec = now.tv_sec;
				vbefore.tv_usec = now.tv_usec;
			} else {
				/* What's the timestamp skip from the previous packet? */
				ts_diff = video->ts - video->prev->ts;
				ts_diff = (ts_diff*1000)/vkhz;
				/* Check if it's time to send */
				gettimeofday(&now, NULL);
				d_s = now.tv_sec - vbefore.tv_sec;
				d_us = now.tv_usec - vbefore.tv_usec;
				if(d_us < 0) {
					d_us += 1000000;
					--d_s;
				}
				passed = d_s*1000000 + d_us;
				if(passed < (ts_diff-5000)) {
					vsent = FALSE;
				} else {
					/* Update the reference time */
					vbefore.tv_usec += ts_diff%1000000;
					if(vbefore.tv_usec > 1000000) {
						vbefore.tv_sec++;
						vbefore.tv_usec -= 1000000;
					}
					if(ts_diff/1000000 > 0) {
						vbefore.tv_sec += ts_diff/1000000;
						vbefore.tv_usec -= ts_diff/1000000;
					}
					/* There may be multiple packets with the same timestamp, send them all */
					uint64_t ts = video->ts;
					while(video && video->ts == ts) {
						/* Send now */
						fseek(vfile, video->offset, SEEK_SET);
						bytes = fread(buffer, sizeof(char), video->len, vfile);
						if(bytes != video->len)
							JANUS_LOG(LOG_WARN, "Didn't manage to read all the bytes we needed (%d < %d)...\n", bytes, video->len);
						/* Update payload type */
						janus_rtp_header *rtp = (janus_rtp_header *)buffer;
						rtp->type = video_pt;
						if(gateway != NULL) {
              janus_plugin_rtp prtp = { .video = TRUE, .buffer = (char *)buffer, .length = bytes };
              janus_plugin_rtp_extensions_reset(&prtp.extensions);
              gateway->relay_rtp(session->handle, &prtp);
            }
						video = video->next;
					}
					vsent = TRUE;
				}
			}
		}
	}

	g_free(buffer);

	/* Get rid of the indexes */
	janus_auviousrecordplay_frame_packet *tmp = NULL;
	audio = session->aframes;
	while(audio) {
		tmp = audio->next;
		g_free(audio);
		audio = tmp;
	}
	session->aframes = NULL;
	video = session->vframes;
	while(video) {
		tmp = video->next;
		g_free(video);
		video = tmp;
	}
	session->vframes = NULL;

	if(afile)
		fclose(afile);
	afile = NULL;
	if(vfile)
		fclose(vfile);
	vfile = NULL;

	/* Remove from the list of viewers */
	janus_mutex_lock(&rec->mutex);
	rec->viewers = g_list_remove(rec->viewers, session);
	janus_mutex_unlock(&rec->mutex);

	/* Tell the core to tear down the PeerConnection, hangup_media will do the rest */
	gateway->close_pc(session->handle);

	janus_refcount_decrease(&rec->ref);
	janus_refcount_decrease(&session->ref);

	JANUS_LOG(LOG_INFO, "Leaving playout thread\n");
	g_thread_unref(g_thread_self());
	return NULL;
}
