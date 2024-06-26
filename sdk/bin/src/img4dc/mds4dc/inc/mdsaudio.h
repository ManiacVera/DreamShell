/*
	mdsaudio.h - Media Descriptor Header Support Writer for Audio/Data images format
	
	Created by [big_fury]SiZiOUS / Dreamcast-Scene 2007
	Version 0.1 - 05/06/07
	
	Based on specs by Henrik Stokseth <hensto AT c2i DOT net>
*/

#ifndef __MDSAUDIO__H__
#define __MDSAUDIO__H__

/*
	Structure d'un "MDS" AUDIO / DATA:
	
	- Header fixe (92 bytes)
	- Infos de session CDDA (48 bytes)
	  - Infos MDS_LEAD-IN_TRACK_FIRST (80 bytes)
	  - Infos MDS_LEAD-IN_TRACK_LAST (80 bytes)
	  - Infos MDS_LEAN-IN_TRACK_LEADOUT (76 bytes)
	    - n pistes CDDA (80 bytes)
	- Infos de session DATA (244 bytes)
	  - Infos MDS_LEAD-IN_TRACK_FIRST (80 bytes)
	  - Infos MDS_LEAD-IN_TRACK_LAST (80 bytes)
	  - Infos MDS_LEAN-IN_TRACK_LEADOUT (76 bytes)
	    - 1 piste DATA (80 bytes)
	- data_track_infos_header_start (108 bytes)
	    - n valeurs msinfo de chaques pistes cdda (8 bytes chaques)
	- Footer (22 bytes)
*/	  

/* Donn�es sources pour la construction du fichier MDS "Media Descriptor" */

/* Header */

#define AD_MDS_HEADER_ENTRIES 26
#define AD_MDS_HEADER_SIZE 92
static const unsigned int ad_mds_header[AD_MDS_HEADER_ENTRIES][2] = {
		{0x00000, 0x4d}, {0x00001, 0x45}, {0x00002, 0x44}, {0x00003, 0x49}, {0x00004, 0x41}, {0x00005, 0x20}, {0x00006, 0x44},
		{0x00007, 0x45}, {0x00008, 0x53}, {0x00009, 0x43}, {0x0000a, 0x52}, {0x0000b, 0x49}, {0x0000c, 0x50}, {0x0000d, 0x54},
		{0x0000e, 0x4f}, {0x0000f, 0x52}, {0x00010, 0x01}, {0x00011, 0x04}, {0x00012, 0x00}, {0x00014, 0x02}, {0x00016, 0x02},
		{0x00050, 0x58}, {0x00058, 0x6a}, {0x00059, 0xff}, {0x0005a, 0xff}, {0x0005b, 0xff} /* 0x0012 a �t� patch� (0x02 � l'origine) ... */
};

/* Session 01 : Audio (CD-DA) */

#define AD_CDDA_SESSION_INFOS_ENTRIES 28
#define AD_CDDA_SESSION_INFOS_SIZE 48
static const unsigned int ad_cdda_session_infos[AD_CDDA_SESSION_INFOS_ENTRIES][2] = { /* Toutes les valeurs 0xff sont � remplacer. */
		{0x00000, 0xff}, {0x00001, 0xff}, {0x00002, 0xff}, {0x00003, 0xff}, {0x00004, 0x01}, {0x00006, 0xff}, {0x00007, 0x03},
		{0x00008, 0x01}, {0x0000a, 0xff}, {0x00010, 0x88}, {0x00014, 0xff}, {0x00015, 0xff}, {0x00016, 0xff}, {0x00017, 0xff},
		{0x00018, 0xff}, {0x00019, 0xff}, {0x0001a, 0xff}, {0x0001b, 0xff}, {0x0001c, 0x02}, {0x0001e, 0x05}, {0x0001f, 0x03},
		{0x00020, 0xff}, {0x00022, 0xff}, {0x00028, 0xff}, {0x00029, 0xff}, {0x0002a, 0xff}, {0x0002b, 0xff}, {0x0002e, 0x10},
};

#define AD_CDDA_LEAD_IN_TRACK_FIRST_ENTRIES 3
#define AD_CDDA_LEAD_IN_TRACK_FIRST_SIZE 80
static const unsigned int ad_cdda_lead_in_track_first[AD_CDDA_LEAD_IN_TRACK_FIRST_ENTRIES][2] = {
		{0x00000, 0xa0}, {0x00005, 0x01}, {0x0004e, 0x10}
};

#define AD_CDDA_LEAD_IN_TRACK_LAST_ENTRIES 3
#define AD_CDDA_LEAD_IN_TRACK_LAST_SIZE 80
static const unsigned int ad_cdda_lead_in_track_last[AD_CDDA_LEAD_IN_TRACK_LAST_ENTRIES][2] = { /* valeur 0xff � modifier (nombre de pistes cdda) */
		{0x00000, 0xa1}, {0x00005, 0xff}, {0x0004e, 0x10}
};

#define AD_CDDA_LEAD_IN_TRACK_LEADOUT_ENTRIES 4
#define AD_CDDA_LEAD_IN_TRACK_LEADOUT_SIZE 76
static const unsigned int ad_cdda_lead_in_track_leadout[AD_CDDA_LEAD_IN_TRACK_LEADOUT_ENTRIES][2] = { /* 0xff � modifier */
		{0x00000, 0xa2}, {0x00005, 0xff}, {0x00006, 0xff}, {0x00007, 0xff}
};

#define AD_CDDA_TRACK_ENTRIES 26
#define AD_CDDA_TRACK_SIZE 80
static const unsigned int ad_cdda_track[AD_CDDA_TRACK_ENTRIES][2] = { /* valeurs 0xff e 0xee � modifier */
		{0x00000, 0xa9}, {0x00002, 0x10}, {0x00004, 0xff}, {0x00009, 0xee}, {0x0000a, 0xee}, {0x0000b, 0xee}, {0x0000c, 0xff},
		{0x0000d, 0xff}, {0x0000e, 0xff}, {0x0000f, 0xff}, {0x00010, 0x30}, {0x00011, 0x09}, {0x00012, 0x02}, {0x00024, 0xff},
		{0x00025, 0xff}, {0x00026, 0xff}, {0x00027, 0xff}, {0x00028, 0xee}, {0x00029, 0xee}, {0x0002a, 0xee}, {0x0002b, 0xee},
		{0x00030, 0x01}, {0x00034, 0xff}, {0x00035, 0xff}, {0x00036, 0xff}, {0x00037, 0xff}
};

/* Session 02 : Donn�es (DATA en MODE 2 FORM 1) */

#define AD_DATA_SESSION_INFOS_ENTRIES 22
#define AD_DATA_SESSION_INFOS_SIZE 244
static const unsigned int ad_data_session_infos[AD_DATA_SESSION_INFOS_ENTRIES][2] = { /* valeurs 0xff � modifier (msf ici ...) */
		{0x00002, 0x50}, {0x00004, 0xb0}, {0x00005, 0xff}, {0x00006, 0xff}, {0x00007, 0xff}, {0x00008, 0x03}, {0x00009, 0x4f},
		{0x0000a, 0x3b}, {0x0000b, 0x4a}, {0x00052, 0x50}, {0x00054, 0xc0}, {0x00055, 0x46}, {0x00057, 0x9e}, {0x00059, 0x61},
		{0x0005a, 0x22}, {0x0005b, 0x17}, {0x000a2, 0x50}, {0x000a4, 0xc1}, {0x000a5, 0x48}, {0x000a6, 0x58}, {0x000a7, 0xb8},
		{0x000f2, 0x14}
};

#define AD_DATA_LEAD_IN_TRACK_FIRST_ENTRIES 4
#define AD_DATA_LEAD_IN_TRACK_FIRST_SIZE 80
static const unsigned int ad_data_lead_in_track_first[AD_DATA_LEAD_IN_TRACK_FIRST_ENTRIES][2] = { /* 0xff � modif */
		{0x00000, 0xa0}, {0x00005, 0xff}, {0x00006, 0x20}, {0x0004e, 0x14}
};

#define AD_DATA_LEAD_IN_TRACK_LAST_ENTRIES 3
#define AD_DATA_LEAD_IN_TRACK_LAST_SIZE 80
static const unsigned int ad_data_lead_in_track_last[AD_DATA_LEAD_IN_TRACK_LAST_ENTRIES][2] = { /* 0xff � modif */
		{0x00000, 0xa1}, {0x00005, 0xff}, {0x0004e, 0x14}
};

#define AD_DATA_LEAD_IN_TRACK_LEADOUT_ENTRIES 4
#define AD_DATA_LEAD_IN_TRACK_LEADOUT_SIZE 76
static const unsigned int ad_data_lead_in_track_leadout[AD_DATA_LEAD_IN_TRACK_LEADOUT_ENTRIES][2] = { /* 0xff � modif (msf ici) */
		{0x00000, 0xa2}, {0x00005, 0xff}, {0x00006, 0xff}, {0x00007, 0xff}
};

#define AD_DATA_TRACK_ENTRIES 26
#define AD_DATA_TRACK_SIZE 80
static const unsigned int ad_data_track[AD_DATA_TRACK_ENTRIES][2] = { /* 0xee et 0xff � modif */
		{0x00000, 0xec}, {0x00002, 0x14}, {0x00004, 0xff}, {0x00009, 0xff}, {0x0000a, 0xff}, {0x0000b, 0xff}, {0x0000c, 0xee},
		{0x0000d, 0xee}, {0x0000e, 0xee}, {0x0000f, 0xee}, {0x00010, 0x30}, {0x00011, 0x09}, {0x00012, 0x02}, {0x00024, 0xff},
		{0x00025, 0xff}, {0x00026, 0xff}, {0x00027, 0xff}, {0x00028, 0xee}, {0x00029, 0xee}, {0x0002a, 0xee}, {0x0002b, 0xee},
		{0x00030, 0x01}, {0x00034, 0xff}, {0x00035, 0xff}, {0x00036, 0xff}, {0x00037, 0xff}
};

#define AD_DATA_TRACK_INFOS_HEADER_START_ENTRIES 10
#define AD_DATA_TRACK_INFOS_HEADER_START_SIZE 108
static const unsigned int ad_data_track_infos_header_start[AD_DATA_TRACK_INFOS_HEADER_START_ENTRIES][2] = {
		{0x00002, 0x54}, {0x00004, 0xb0}, {0x00005, 0xff}, {0x00006, 0xff}, {0x00007, 0xff}, {0x00008, 0x01}, {0x00009, 0x4f},
		{0x0000a, 0x3b}, {0x0000b, 0x4a}, {0x00068, 0x96}
};

#define AD_DATA_TRACK_INFOS_HEADER_END_ENTRIES 5
#define AD_DATA_TRACK_INFOS_HEADER_END_SIZE 60
static const unsigned int ad_data_track_infos_header_end[AD_DATA_TRACK_INFOS_HEADER_END_ENTRIES][2] = { /* 0xff � modif */
		{0x0002c, 0x96}, {0x00030, 0xff}, {0x00031, 0xff}, {0x00032, 0xff}, {0x00033, 0xff}
};

/* Footer */

#define AD_MDS_FOOTER_ENTRIES 9
#define AD_MDS_FOOTER_SIZE 22
static const unsigned int ad_mds_footer[AD_MDS_FOOTER_ENTRIES][2] = { /* 0xff � modif */
		{0x00000, 0xff}, {0x00001, 0xff}, {0x00002, 0xff}, {0x00003, 0xff}, {0x00010, 0x2a}, {0x00011, 0x2e}, {0x00012, 0x6d},
		{0x00013, 0x64}, {0x00014, 0x66}
};

void ad_write_data_session_infos(FILE* mds, int cdda_session_sectors_count);
void ad_write_data_track_infos(FILE* mds, int cdda_session_sectors_count, int cdda_tracks_count);
void ad_write_data_lead_in_track_first_infos(FILE* mds, int cdda_tracks_count);
void ad_write_data_lead_in_track_last_infos(FILE* mds, int cdda_tracks_count);
void ad_write_data_lead_in_track_leadout_infos(FILE* mds, int cdda_session_sectors_count, int data_session_sectors_count);
void ad_write_data_track_infos_header_start(FILE* mds);
void ad_write_data_track_sectors_count(FILE* mds, unsigned int data_session_sectors_count);

void ad_write_mds_header(FILE* mds);
void ad_write_mds_footer(FILE* mds, int cdda_tracks_count);

void ad_write_cdda_session_infos(FILE* mds, int cdda_session_sectors_count, int cdda_tracks_count, int data_session_sectors_count);
void ad_write_cdda_track_infos(FILE* mds, int track_num, int cdda_tracks_count, int msinfo);
void ad_write_cdda_lead_in_track_leadout_infos(FILE* mds, int cdda_session_sectors_count);
void ad_write_cdda_lead_in_track_last_infos(FILE* mds, int cdda_tracks_count);
void ad_write_cdda_lead_in_track_first_infos(FILE* mds);
void ad_write_cdda_session_infos(FILE* mds, int cdda_session_sectors_count, int cdda_tracks_count, int data_session_sectors_count);
void ad_write_cdda_track_sectors_count(FILE* mds, unsigned int cdda_sectors_count);

#endif // __MDSAUDIO__H__

