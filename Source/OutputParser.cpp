/*
 * Copyright 2017. All rights reserved.
 * Distributed under the terms of the MIT license.
 *
 * Author:
 *	Humdinger, humdingerb@gmail.com
 */

#include "Constants.h"
#include "OutputParser.h"

#include <Catalog.h>
#include <DateTimeFormat.h>
#include <DurationFormat.h>
#include <StringList.h>
#include <TimeFormat.h>

#include <parsedate.h>
#include <stdio.h>
#include <stdlib.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Parser"


OutputParser::OutputParser(float& noteProgress, BString& noteEta):
	progress(noteProgress),
	eta(noteEta)
{
	Reset();
}


OutputParser::~OutputParser()
{
}


int32
OutputParser::ParseLine(BString& text, BString newline)
{
	int32 resultNewline;
	int32 resultText;
printf("New line: %s\n", newline.String());
	// detect progress of makeisofs
	resultNewline = newline.FindFirst("done, estimate finish");
	if (resultNewline != B_ERROR) {
		// get the percentage
		BStringList percentList;
		newline.Split("%", true, percentList);
		printf("mkisofs percentage: %s\n", percentList.StringAt(0).String());
		progress = atof(percentList.StringAt(0)) / 100;

		// get the ETA
		BString when;
		int32 charCount = newline.FindFirst("finish");
		newline.CopyInto(when, charCount + 6, newline.CountChars());

		const char* dateformat("A B d H:M:S Y");
		set_dateformats(&dateformat);
		bigtime_t finishTime = parsedate(when, -1);
		bigtime_t now = (bigtime_t)real_time_clock_usecs();

		BString duration;
		BDurationFormat formatter;
		// add 1 sec, otherwise the last second of the progress isn't shown...
		formatter.Format(duration, now - 1000000LL, finishTime * 1000000LL);
		eta = B_TRANSLATE("Finished in %duration%");
		eta.ReplaceFirst("%duration%", duration);

		// print on top of the last line (not if this is the first progress line)
		resultText = text.FindFirst("done, estimate finish");
		if (resultText != B_ERROR) {
			int32 offset = text.FindLast("\n");
			if (offset != B_ERROR)
				text.Remove(offset, text.CountChars() - offset);
		}
		text << "\n" << newline;
		return PERCENT;
	}

	// detect progress of cdrecord -v
	resultNewline = newline.FindFirst(" MB written (fifo");
	if (resultNewline != B_ERROR) {
		// calculate percentage
		BStringList sizeList;
		newline.Split(" ", true, sizeList);
		float currentSize = atof(sizeList.StringAt(2));
		float targetSize = atof(sizeList.StringAt(4));
		progress = currentSize / targetSize;
	printf("cdrecord, current: %f, target: %f, percentage: %f\n",
		currentSize, targetSize, progress);

		// calculate ETA
		bigtime_t now = (bigtime_t)real_time_clock_usecs();
		float speed = ((currentSize - fLastSize) * 1000000.0
			/ (now - fLastTime)); // MB/s
		float secondsLeft = (targetSize - currentSize) / speed;
		fLastTime = now;
		fLastSize = currentSize;
	printf("cdrecord, speed: %f, seconds left: %f\n", speed, secondsLeft);

		BString duration;
		BDurationFormat formatter;
		formatter.Format(duration, now, now + ((bigtime_t)secondsLeft * 1000000LL));
		eta = B_TRANSLATE("Finished in %duration%");
		eta.ReplaceFirst("%duration%", duration);
	printf("ETA: %s\n\n", eta.String());

		// print on top of the last line (not if this is the first progress line)
		resultText = text.FindFirst(" MB written (fifo");
		if (resultText != B_ERROR) {
			int32 offset = text.FindLast("\n");
			if (offset != B_ERROR)
				text.Remove(offset, text.CountChars() - offset);
		}
		text << "\n" << newline;
		return PERCENT;
	}

	// replace the newline string
//	resultNewline = newline.FindFirst(
//		"Last chance to quit, starting real write in");
//	if (resultNewline != B_ERROR) {
//		text << "\n" << "Waiting...";
//		return CHANGE;
//	}

	return NOCHANGE;
}


void
OutputParser::Reset()
{
	fLastTime = (bigtime_t)real_time_clock_usecs() - 1000000LL; // now - 1 sec
	fLastSize = 0;
}