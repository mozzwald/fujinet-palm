#ifndef VC_APP_IDENTITY_H
#define VC_APP_IDENTITY_H
/* Distinct-reference RAM checker keeps separate preferences and databases. */
#ifdef VC_ROMPROBE_REFERENCE
#define VC_CREATOR 'FNVD'
#define VC_RESULTS_DB "ROMProbeResults"
#define VC_CONTROL_DB "ROMProbeControl"
#else
#define VC_CREATOR 'FNVC'
#define VC_RESULTS_DB "VisorCheckResults"
#define VC_CONTROL_DB "VisorCheckControl"
#endif
#endif
