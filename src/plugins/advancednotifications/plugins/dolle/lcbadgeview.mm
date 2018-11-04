/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2014  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#import "lcbadgeview.h"
#include <QDebug>
#import <Cocoa/Cocoa.h>

namespace
{

static const int kMaxBadgesDisplayed = 6;
static const int kRowsInOneColumn = 3;
static const CGFloat kLabelsDyOffset = 10;
static const int kFontSize = 24;

}

@implementation LCBadgeView

- (void)dealloc
{
	if (badges)
		[badges release];
	if (colors)
		[colors release];
	[super dealloc];
}

- (BOOL)displayBadges: (NSArray*)b andColors: (NSArray*)c
{
	@synchronized (badges)
	{
		if (badges && [badges isEqualToArray: b])
			return NO;

		if ([b count] != [c count])
		{
			qDebug () << Q_FUNC_INFO << "Badge labels count not equal colors count.";
			return NO;
		}
		badges = [b copy];
		colors = [c copy];
	}
	[[NSApp dockTile] display];
	return YES;
}

- (void)drawRect: (NSRect)rect
{
	NSRect boundary = rect;

	[[NSApp applicationIconImage] drawInRect: boundary
									fromRect: NSZeroRect
								   operation: NSCompositingOperationCopy
									fraction: 1.0];

	if (badges == nil)
		return;

	boundary.size.width -= 2.0;
	boundary.size.height -= 2.0;

	CGFloat badgeBorderDx = 0.0;

	{
		NSSize sz;
		[self elideString: @"1"
				 forWidth: boundary.size.width
				  outSize: &sz];
		badgeBorderDx = sz.height / 4.0;
	}

	const bool isTwoCols = [badges count] > kRowsInOneColumn;
	const CGFloat maxWidth = boundary.size.width / (isTwoCols ? 2.0 : 1.0) - (badgeBorderDx * 2);

	@synchronized (badges)
	{
		CGFloat yOffset = boundary.size.height;

		for (NSUInteger i = 0; i < [badges count]; ++i)
		{
			__strong NSString* const badgeSrc = badges [i];
			__strong NSColor* const badgeColor = colors [i];

			if (i % kRowsInOneColumn == 0)
				yOffset = boundary.size.height;

			const NSUInteger column = isTwoCols // 0 - right, 1 - left %)
					? i / kRowsInOneColumn
					: 0;

			const CGFloat columnMaxX = column == 0 ? boundary.size.width : boundary.size.width / 2.0;

			NSSize badgeSize;
			NSString* badge = [self elideString: badgeSrc
									   forWidth: maxWidth
										outSize: &badgeSize];
			if (badge == nil)
			{
				continue;
			}

			if (badgeSize.width > columnMaxX)
				badgeSize.width = columnMaxX;
			if (badgeSize.height > boundary.size.height)
				badgeSize.height = boundary.size.height;


			yOffset -= badgeSize.height;
			if (yOffset < 0.0)
				break;

			NSRect badgeRect = NSMakeRect (column == 0 ? columnMaxX - badgeSize.width : badgeBorderDx,
										   yOffset,
										   badgeSize.width,
										   badgeSize.height);

			NSRect badgeWithBorderRect = CGRectInset (badgeRect, -badgeBorderDx, -1.5);
			const CGFloat maxX = CGRectGetMaxX (badgeWithBorderRect) + 0.1;
			const CGFloat maxY = CGRectGetMaxY (badgeWithBorderRect) + 0.1;

			if (maxX > columnMaxX || maxY > boundary.size.height)
			{
				const CGFloat dx = maxX > columnMaxX ? -maxX + columnMaxX : 0.0;
				const CGFloat dy = maxY > boundary.size.height ? -maxY + boundary.size.height : 0.0;

				badgeWithBorderRect = CGRectOffset (badgeWithBorderRect, dx, dy);
				badgeRect = CGRectOffset (badgeRect, dx, dy);

				if (badgeWithBorderRect.origin.x < 0.0)
				{
					badgeWithBorderRect.size.width += badgeWithBorderRect.origin.x;
					badgeWithBorderRect.origin.x = 0.0;
				}
				if (badgeRect.origin.x < 0.0)
				{
					badgeRect.size.width += badgeRect.origin.x;
					badgeRect.origin.x = 0.0;
				}
			}

			if (!CGRectContainsRect (boundary, badgeWithBorderRect))
				break;

			[NSGraphicsContext saveGraphicsState];

			NSShadow* shadow = [[[NSShadow alloc] init] autorelease];
			shadow.shadowColor = [[NSColor blackColor] colorWithAlphaComponent:0.4];
			shadow.shadowBlurRadius = 1;
			shadow.shadowOffset = NSMakeSize (0, -1);
			[shadow set];

			[NSGraphicsContext restoreGraphicsState];

			NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect: badgeWithBorderRect
																 xRadius: badgeSize.height / 2.0
																 yRadius: badgeSize.height / 2.0];

			NSArray* gradientColors = [self getGradientColorsForColor: badgeColor];
			NSGradient* gradient = [[NSGradient alloc] initWithStartingColor: gradientColors [0]
																 endingColor: gradientColors [1]];

			[gradient drawInBezierPath: path angle: -90.0];
			[gradient release];

			NSMutableParagraphStyle* paragraphStyle = [[[NSMutableParagraphStyle alloc] init] autorelease];
			[paragraphStyle setAlignment: NSTextAlignmentCenter];

			const CGFloat brightness =
					0.30 * [badgeColor redComponent] +
					0.59 * [badgeColor greenComponent] +
					0.11 * [badgeColor blueComponent];

			NSColor* textColor = brightness > 0.5
					? [NSColor blackColor]
					: [NSColor whiteColor];

			NSDictionary* textAttributes = @{
					NSForegroundColorAttributeName: textColor,
					NSParagraphStyleAttributeName: paragraphStyle,
					NSFontAttributeName: [NSFont systemFontOfSize: kFontSize]
			};
			[badge drawInRect: badgeRect withAttributes: textAttributes];
			yOffset -= kLabelsDyOffset;
		}
	}
}

- (NSString*) elideString: (NSString*)s forWidth: (CGFloat)width outSize: (NSSize*)pSize
{
	if (s == nil || width <= 0.0 || pSize == nil)
		return nil;

	static NSString* const elidePrefix = @"#";
	static const NSUInteger elideLen = [elidePrefix length];
	static NSDictionary* const attrs = [@{NSFontAttributeName: [NSFont systemFontOfSize: kFontSize]} retain];

	NSSize sz = [s sizeWithAttributes: attrs];
	const auto borderDx = sz.height / 2.0;

	if (sz.width + borderDx <= width)
	{
		*pSize = sz;
		return s;
	}

	NSMutableString* result = [NSMutableString stringWithCapacity: [s length] + elideLen];
	[result appendString:elidePrefix];

	[s enumerateSubstringsInRange: NSMakeRange (0, [s length])
						  options: (NSStringEnumerationReverse | NSStringEnumerationByComposedCharacterSequences)
					   usingBlock: ^(NSString *substring, NSRange substringRange, NSRange enclosingRange, BOOL *stop)
	{
		Q_UNUSED (substringRange);
		Q_UNUSED (enclosingRange);

		[result insertString:substring atIndex:elideLen];

		NSSize size = [result sizeWithAttributes: attrs];

		if (size.width <= 0.0 || size.width + borderDx > width)
		{
			[result deleteCharactersInRange:NSMakeRange (elideLen, 1)];
			*pSize = size = [result sizeWithAttributes: attrs];
			*stop = YES;
		}
	}];
	return [[result copy] autorelease];
}

- (int)maxBadges
{
	return kMaxBadgesDisplayed;
}

- (NSArray*)getGradientColorsForColor: (NSColor*)initialColor
{
	CGFloat hue = 0.0;
	CGFloat sat = 0.0;
	CGFloat br = 0.0;
	CGFloat alp = 0.0;

	[initialColor getHue: &hue saturation: &sat brightness: &br alpha: &alp];

	sat /= 1.8;
	br = std::max (0.0, br - 0.08);
	NSColor * cUp = [NSColor colorWithCalibratedHue: hue saturation: sat brightness: br alpha: alp];
	NSColor * cDown = [initialColor shadowWithLevel: 0.25];
	
	return @[cUp, cDown];
}

@end
