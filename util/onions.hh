#pragma once

#include <string>
#include <map>
#include <list>
#include <iostream>
#include <vector>


typedef enum onion {
    oDET,
    oOPE,
    oAGG,
    oSWP,
    oPLAIN,
    oBESTEFFORT,
    oINVALID,
    oFHDET,
    oFHOPE,
} onion;

//Sec levels ordered such that
// if a is less secure than b.
// a appears before b
// (note, this is not "iff")

enum class SECLEVEL {
    INVALID,
    PLAINVAL,
    OPE,
    DETJOIN,
    DET,
    SEARCH,
    HOM,
    RND,
    FHDET,
    FHOPE,
};

//Onion layouts - initial structure of onions
typedef std::map<onion, std::vector<SECLEVEL> > onionlayout;

static onionlayout PLAIN_ONION_LAYOUT = {
    {oPLAIN, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL})}
};

static onionlayout NUM_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE})},
    {oAGG, std::vector<SECLEVEL>({SECLEVEL::HOM})}
};

static onionlayout FH_NUM_ONION_LAYOUT = {
	{oPLAIN, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL})},
    {oFHDET, std::vector<SECLEVEL>({SECLEVEL::FHDET})},

	// This layout is useless.
    {oFHOPE, std::vector<SECLEVEL>({SECLEVEL::FHOPE})},
};

static onionlayout BEST_EFFORT_NUM_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE})},
    {oAGG, std::vector<SECLEVEL>({SECLEVEL::HOM})},
    // Requires SECLEVEL::DET, otherwise you will have to implement
    // encoding for negative numbers in SECLEVEL::RND.
    {oPLAIN, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL})}
};

static onionlayout STR_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE})},
    // {oSWP, std::vector<SECLEVEL>({SECLEVEL::SEARCH})}
    // {oSWP, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL, SECLEVEL::DET,
                                  // SECLEVEL::RND})}
};

static onionlayout FH_STR_ONION_LAYOUT = {
	{oPLAIN, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL})},
    {oFHDET, std::vector<SECLEVEL>({SECLEVEL::FHDET})},
    {oFHOPE, std::vector<SECLEVEL>({SECLEVEL::FHOPE})},
};

static onionlayout BEST_EFFORT_STR_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE})},
    // {oSWP, std::vector<SECLEVEL>({SECLEVEL::SEARCH})},
    // {oSWP, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL, SECLEVEL::DET,
    //                              SECLEVEL::RND})},
    // HACK: RND_str expects the data to be a multiple of 16, so we use
    // DET (it supports decryption UDF) to handle the padding for us.
    {oPLAIN, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL})}
};

typedef std::map<onion, SECLEVEL>  OnionLevelMap;

enum class SECURITY_RATING {PLAIN, BEST_EFFORT, SENSITIVE};
