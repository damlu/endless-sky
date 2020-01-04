/* SuitInfoPanel.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "SuitInfoPanel.h"

#include "Command.h"
#include "Dialog.h"
#include "Font.h"
#include "FontSet.h"
#include "Format.h"
#include "GameData.h"
#include "Information.h"
#include "Interface.h"
#include "LineShader.h"
#include "LogbookPanel.h"
#include "Messages.h"
#include "MissionPanel.h"
#include "OutlineShader.h"
#include "PlayerInfo.h"
#include "PlayerInfoPanel.h"
#include "Rectangle.h"
#include "ShipInfoPanel.h"
#include "Suit.h"
#include "Sprite.h"
#include "SpriteShader.h"
#include "Table.h"
#include "UI.h"

#include <algorithm>

using namespace std;

namespace {
	const double WIDTH = 250.;
}


SuitInfoPanel::SuitInfoPanel(PlayerInfo &player, int index)
	: player(player), suitIt(player.Suits().begin()), canEdit(player.GetPlanet())
{
	SetInterruptible(false);
	
	// If a valid suit index was given, show that suit.
	if(static_cast<unsigned>(index) < player.Suits().size())
		suitIt += index;
	else if(player.Flagsuit())
	{
		// Find the player's flagsuit. It may not be first in the list, if the
		// first item in the list cannot be a flagsuit.
		while(suitIt != player.Suits().end() && suitIt->get() != player.Flagsuit())
			++suitIt;
	}
	
	UpdateInfo();
}



void SuitInfoPanel::Draw()
{
	// Dim everything behind this panel.
	DrawBackdrop();
	
	// Fill in the information for how this interface should be drawn.
	Information interfaceInfo;
	interfaceInfo.SetCondition("suit tab");
	if(canEdit && (suitIt != player.Suits().end())
			&& (suitIt->get() != player.Flagsuit()))
	{
		interfaceInfo.SetCondition("show disown");
	}
	else if(!canEdit)
	{
		interfaceInfo.SetCondition("show dump");
		if(CanDump())
			interfaceInfo.SetCondition("enable dump");
	}
	if(player.Suits().size() > 1)
		interfaceInfo.SetCondition("five buttons");
	else
		interfaceInfo.SetCondition("three buttons");
	if(player.HasLogs())
		interfaceInfo.SetCondition("enable logbook");
	
	// Draw the interface.
	const Interface *interface = GameData::Interfaces().Get("info panel");
	interface->Draw(interfaceInfo, this);
	
	// Draw all the different information sections.
	zones.clear();
	commodityZones.clear();
	plunderZones.clear();
	DrawSuitStats(interface->GetBox("suit stats"));
	DrawBodymods(interface->GetBox("bodymods"));
	DrawBodymodSlots(interface->GetBox("bodymod slots"));
	
	// If the player hovers their mouse over a suit attribute, show its tooltip.
	info.DrawTooltips();
}



bool SuitInfoPanel::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress)
{
	bool shift = (mod & KMOD_SHIFT);
	if(key == 'd' || key == SDLK_ESCAPE || (key == 'w' && (mod & (KMOD_CTRL | KMOD_GUI))))
		GetUI()->Pop(this);
	else if(!player.Suits().empty() && ((key == 'p' && !shift) || key == SDLK_LEFT || key == SDLK_UP))
	{
		if(suitIt == player.Suits().begin())
			suitIt = player.Suits().end();
		--suitIt;
		UpdateInfo();
	}
	else if(!player.Suits().empty() && (key == 'n' || key == SDLK_RIGHT || key == SDLK_DOWN))
	{
		++suitIt;
		if(suitIt == player.Suits().end())
			suitIt = player.Suits().begin();
		UpdateInfo();
	}
	else if(!player.Ships().empty() && key == 's')
	{
		GetUI()->Pop(this);
		GetUI()->Push(new ShipInfoPanel(player, 0));
	}
	else if(key == 'i' || command.Has(Command::INFO))
	{
		GetUI()->Pop(this);
		GetUI()->Push(new PlayerInfoPanel(player));
	}
	else if(key == 'R' || (key == 'r' && shift))
		GetUI()->Push(new Dialog(this, &SuitInfoPanel::Rename, "Change this suit's name?"));
	else if(canEdit && key == 'D')
	{
		if(suitIt->get() != player.Flagsuit())
			GetUI()->Push(new Dialog(this, &SuitInfoPanel::Disown, "Are you sure you want to disown \""
				+ suitIt->get()->Name()
				+ "\"? Disowning a suit rather than selling it means you will not get any money for it."));
	}
	else if(command.Has(Command::MAP) || key == 'm')
		GetUI()->Push(new MissionPanel(player));
	else if(key == 'l' && player.HasLogs())
		GetUI()->Push(new LogbookPanel(player));
	else
		return false;
	
	return true;
}



bool SuitInfoPanel::Click(int x, int y, int clicks)
{
	if(suitIt == player.Suits().end())
		return true;
	
	draggingIndex = -1;
	if(canEdit && hoverIndex >= 0)
		draggingIndex = hoverIndex;
	
	selectedCommodity.clear();
	selectedPlunder = nullptr;
	Point point(x, y);
	for(const auto &zone : commodityZones)
		if(zone.Contains(point))
			selectedCommodity = zone.Value();
	for(const auto &zone : plunderZones)
		if(zone.Contains(point))
			selectedPlunder = zone.Value();
	
	return true;
}



bool SuitInfoPanel::Hover(int x, int y)
{
	Point point(x, y);
	info.Hover(point);
	return Hover(point);
}



bool SuitInfoPanel::Drag(double dx, double dy)
{
	return Hover(hoverPoint + Point(dx, dy));
}



bool SuitInfoPanel::Release(int x, int y)
{
//	if(draggingIndex >= 0 && hoverIndex >= 0 && hoverIndex != draggingIndex)
//		(**suitIt).GetArmament().Swap(hoverIndex, draggingIndex);
//
	draggingIndex = -1;
	return true;
}



void SuitInfoPanel::UpdateInfo()
{
	draggingIndex = -1;
	hoverIndex = -1;
	if(suitIt == player.Suits().end())
		return;
	
	const Suit &suit = **suitIt;
	info.Update(suit, player.FleetDepreciation(), player.GetDate().DaysSinceEpoch());
//	if(player.Flagsuit() && &suit != player.Flagsuit())
//		player.Flagsuit()->SetTargetSuit(*suitIt);
	
	bodymods.clear();
	for(const auto &it : suit.Bodymods())
		bodymods[it.first->Category()].push_back(it.first);
}



void SuitInfoPanel::DrawSuitStats(const Rectangle &bounds)
{
	// Check that the specified area is big enough.
	if(bounds.Width() < WIDTH)
		return;
	
	// Colors to draw with.
	Color dim = *GameData::Colors().Get("medium");
	Color bright = *GameData::Colors().Get("bright");
	const Suit &suit = **suitIt;
	const Font &font = FontSet::Get(14);
	
	// Table attributes.
	Table table;
	table.AddColumn(0, Table::LEFT);
	table.AddColumn(WIDTH - 20, Table::RIGHT);
	table.SetUnderline(0, WIDTH - 20);
	table.DrawAt(bounds.TopLeft() + Point(10., 8.));
	
	// Draw the suit information.
	table.Draw("suit:", dim);
	table.Draw(font.TruncateMiddle(suit.Name(), WIDTH - 50), bright);
	
	table.Draw("model:", dim);
	table.Draw(suit.ModelName(), bright);
	
	info.DrawAttributes(table.GetRowBounds().TopLeft() - Point(10., 10.));
}


//
//void SuitInfoPanel::DrawBodymodSlots(const Rectangle &bounds)
//{
//	// Check that the specified area is big enough.
//	if(bounds.Width() < WIDTH)
//		return;
//
//	const Suit &suit = **suitIt;
//	const Font &font = FontSet::Get(14);
//
//	// Table attributes.
//	Table table;
//	table.AddColumn(0, Table::LEFT);
//	table.AddColumn(WIDTH - 20, Table::RIGHT);
//	table.SetUnderline(0, WIDTH - 20);
//	table.DrawAt(bounds.TopLeft() + Point(10., 8.));
//
//	info.DrawAttributes(table.GetRowBounds().TopLeft() - Point(10., 10.));
//}
//



void SuitInfoPanel::DrawBodymods(const Rectangle &bounds)
{
	// Check that the specified area is big enough.
	if(bounds.Width() < WIDTH)
		return;
	
	// Colors to draw with.
	Color dim = *GameData::Colors().Get("medium");
	Color bright = *GameData::Colors().Get("bright");
	const Suit &suit = **suitIt;
	
	// Table attributes.
	Table table;
	table.AddColumn(0, Table::LEFT);
	table.AddColumn(WIDTH - 20, Table::RIGHT);
	table.SetUnderline(0, WIDTH - 20);
	Point start = bounds.TopLeft() + Point(10., 8.);
	table.DrawAt(start);

	
	// Draw the bodymods in the same order used in the bodymodder.
	for(const string &category : Bodymod::CATEGORIES)
	{
		auto it = bodymods.find(category);
		if(it == bodymods.end())
			continue;
		
		// Skip to the next column if there is not space for this category label
		// plus at least one bodymod.
		if(table.GetRowBounds().Bottom() + 40. > bounds.Bottom())
		{
			start += Point(WIDTH, 0.);
			if(start.X() + WIDTH - 20 > bounds.Right())
				break;
			table.DrawAt(start);
		}
		
		// Draw the category label.
		table.Draw(category, bright);
		table.Advance();
		for(const Bodymod *bodymod : it->second)
		{
			// Check if we've gone below the bottom of the bounds.
			if(table.GetRowBounds().Bottom() > bounds.Bottom())
			{
				start += Point(WIDTH, 0.);
				if(start.X() + WIDTH - 20 > bounds.Right())
					break;
				table.DrawAt(start);
				table.Draw(category, bright);
				table.Advance();
			}
			
			// Draw the bodymod name and count.
			table.Draw(bodymod->Name(), dim);
			string number = to_string(suit.BodymodCount(bodymod));
			table.Draw(number, bright);
		}
		// Add an extra gap in between categories.
		table.DrawGap(10.);
	}
	
//	// Check if this information spilled over into the cargo column.
//	if(table.GetPoint().X() >= cargoBounds.Left())
//	{
//		double startY = table.GetRowBounds().Top() - 8.;
//		cargoBounds = Rectangle::WithCorners(
//			Point(cargoBounds.Left(), startY),
//			Point(cargoBounds.Right(), max(startY, cargoBounds.Bottom())));
//	}
}


void SuitInfoPanel::DrawBodymodSlots(const Rectangle &bounds)
{
	// Check that the specified area is big enough.
	if(bounds.Width() < WIDTH)
		return;

	const Suit &suit = **suitIt;
	const Font &font = FontSet::Get(14);

	// Table attributes.
	Table table;
	table.AddColumn(0, Table::LEFT);
	table.AddColumn(WIDTH - 20, Table::RIGHT);
	table.SetUnderline(0, WIDTH - 20);
	table.DrawAt(bounds.TopLeft() + Point(10., 8.));

	info.DrawBodymodSlots(table.GetRowBounds().TopLeft() - Point(10., 10.));
}


//void SuitInfoPanel::DrawWeapons(const Rectangle &bounds)
//{
//	// Colors to draw with.
//	Color dim = *GameData::Colors().Get("medium");
//	Color bright = *GameData::Colors().Get("bright");
//	const Font &font = FontSet::Get(14);
//	const Suit &suit = **suitIt;
//
//	// Figure out how much to scale the sprite by.
//	const Sprite *sprite = suit.GetSprite();
//	double scale = 0.;
//	if(sprite)
//		scale = min(1., min((WIDTH - 10) / sprite->Width(), (WIDTH - 10) / sprite->Height()));
//
//	// Figure out the left- and right-most hardpoints on the suit. If they are
//	// too far apart, the scale may need to be reduced.
//	// Also figure out how many weapons of each type are on each side.
//	double maxX = 0.;
//	int count[2][2] = {{0, 0}, {0, 0}};
//	for(const Hardpoint &hardpoint : suit.Weapons())
//	{
//		// Multiply hardpoint X by 2 to convert to sprite pixels.
//		maxX = max(maxX, fabs(2. * hardpoint.GetPoint().X()));
//		++count[hardpoint.GetPoint().X() >= 0.][hardpoint.IsTurret()];
//	}
//	// If necessary, shrink the sprite to keep the hardpoints inside the labels.
//	// The width of this UI block will be 2 * (LABEL_WIDTH + HARDPOINT_DX).
//	static const double LABEL_WIDTH = 150.;
//	static const double LABEL_DX = 95.;
//	static const double LABEL_PAD = 5.;
//	if(maxX > (LABEL_DX - LABEL_PAD))
//		scale = min(scale, (LABEL_DX - LABEL_PAD) / (2. * maxX));
//
//	// Draw the suit, using the black silhouette swizzle.
//	SpriteShader::Draw(sprite, bounds.Center(), scale, 8);
//	OutlineShader::Draw(sprite, bounds.Center(), scale * Point(sprite->Width(), sprite->Height()), Color(.5f));
//
//	// Figure out how tall each part of the weapon listing will be.
//	int gunRows = max(count[0][0], count[1][0]);
//	int turretRows = max(count[0][1], count[1][1]);
//	// If there are both guns and turrets, add a gap of ten pixels.
//	double height = 20. * (gunRows + turretRows) + 10. * (gunRows && turretRows);
//
//	double gunY = bounds.Top() + .5 * (bounds.Height() - height);
//	double turretY = gunY + 20. * gunRows + 10. * (gunRows != 0);
//	double nextY[2][2] = {
//		{gunY + 20. * (gunRows - count[0][0]), turretY + 20. * (turretRows - count[0][1])},
//		{gunY + 20. * (gunRows - count[1][0]), turretY + 20. * (turretRows - count[1][1])}};
//
//	int index = 0;
//	const double centerX = bounds.Center().X();
//	const double labelCenter[2] = {-.5 * LABEL_WIDTH - LABEL_DX, LABEL_DX + .5 * LABEL_WIDTH};
//	const double fromX[2] = {-LABEL_DX + LABEL_PAD, LABEL_DX - LABEL_PAD};
//	static const double LINE_HEIGHT = 20.;
//	static const double TEXT_OFF = .5 * (LINE_HEIGHT - font.Height());
//	static const Point LINE_SIZE(LABEL_WIDTH, LINE_HEIGHT);
//	Point topFrom;
//	Point topTo;
//	Color topColor;
//	bool hasTop = false;
//	for(const Hardpoint &hardpoint : suit.Weapons())
//	{
//		string name = "[empty]";
//		if(hardpoint.GetBodymod())
//			name = font.Truncate(hardpoint.GetBodymod()->Name(), 150);
//
//		bool isRight = (hardpoint.GetPoint().X() >= 0.);
//		bool isTurret = hardpoint.IsTurret();
//
//		double &y = nextY[isRight][isTurret];
//		double x = centerX + (isRight ? LABEL_DX : (-LABEL_DX - font.Width(name)));
//		bool isHover = (index == hoverIndex);
//		font.Draw(name, Point(x, y + TEXT_OFF), isHover ? bright : dim);
//		Point zoneCenter(labelCenter[isRight], y + .5 * LINE_HEIGHT);
//		zones.emplace_back(zoneCenter, LINE_SIZE, index);
//
//		// Determine what color to use for the line.
//		float high = (index == hoverIndex ? .8f : .5f);
//		Color color(high, .75f * high, 0.f, 1.f);
//		if(isTurret)
//			color = Color(0.f, .75f * high, high, 1.f);
//
//		// Draw the line.
//		Point from(fromX[isRight], zoneCenter.Y());
//		Point to = bounds.Center() + (2. * scale) * hardpoint.GetPoint();
//		DrawLine(from, to, color);
//		if(isHover)
//		{
//			topFrom = from;
//			topTo = to;
//			topColor = color;
//			hasTop = true;
//		}
//
//		y += LINE_HEIGHT;
//		++index;
//	}
//	// Make sure the line for whatever hardpoint we're hovering is always on top.
//	if(hasTop)
//		DrawLine(topFrom, topTo, topColor);
//
//	// Re-positioning weapons.
//	if(draggingIndex >= 0)
//	{
//		const Bodymod *bodymod = suit.Weapons()[draggingIndex].GetBodymod();
//		string name = bodymod ? bodymod->Name() : "[empty]";
//		Point pos(hoverPoint.X() - .5 * font.Width(name), hoverPoint.Y());
//		font.Draw(name, pos + Point(1., 1.), Color(0., 1.));
//		font.Draw(name, pos, bright);
//	}
//}



//void SuitInfoPanel::DrawCargo(const Rectangle &bounds)
//{
//	Color dim = *GameData::Colors().Get("medium");
//	Color bright = *GameData::Colors().Get("bright");
//	Color backColor = *GameData::Colors().Get("faint");
//	const Suit &suit = **suitIt;
//
//	// Cargo list.
//	const CargoHold &cargo = (player.Cargo().Used() ? player.Cargo() : suit.Cargo());
//	Table table;
//	table.AddColumn(0, Table::LEFT);
//	table.AddColumn(WIDTH - 20, Table::RIGHT);
//	table.SetUnderline(-5, WIDTH - 15);
//	table.DrawAt(bounds.TopLeft() + Point(10., 8.));
//
//	double endY = bounds.Bottom() - 30. * (cargo.Passengers() != 0);
//	bool hasSpace = (table.GetRowBounds().Bottom() < endY);
//	if((cargo.CommoditiesSize() || cargo.HasBodymods() || cargo.MissionCargoSize()) && hasSpace)
//	{
//		table.Draw("Cargo", bright);
//		table.Advance();
//		hasSpace = (table.GetRowBounds().Bottom() < endY);
//	}
//	if(cargo.CommoditiesSize() && hasSpace)
//	{
//		for(const auto &it : cargo.Commodities())
//		{
//			if(!it.second)
//				continue;
//
//			commodityZones.emplace_back(table.GetCenterPoint(), table.GetRowSize(), it.first);
//			if(it.first == selectedCommodity)
//				table.DrawHighlight(backColor);
//
//			table.Draw(it.first, dim);
//			table.Draw(to_string(it.second), bright);
//
//			// Truncate the list if there is not enough space.
//			if(table.GetRowBounds().Bottom() >= endY)
//			{
//				hasSpace = false;
//				break;
//			}
//		}
//		table.DrawGap(10.);
//	}
//	if(cargo.HasBodymods() && hasSpace)
//	{
//		for(const auto &it : cargo.Bodymods())
//		{
//			if(!it.second)
//				continue;
//
//			plunderZones.emplace_back(table.GetCenterPoint(), table.GetRowSize(), it.first);
//			if(it.first == selectedPlunder)
//				table.DrawHighlight(backColor);
//
//			// For bodymods, show how many of them you have and their total mass.
//			bool isSingular = (it.second == 1 || it.first->Get("installable") < 0.);
//			string name = (isSingular ? it.first->Name() : it.first->PluralName());
//			if(!isSingular)
//				name += " (" + to_string(it.second) + "x)";
//			table.Draw(name, dim);
//
//			double mass = it.first->Mass() * it.second;
//			table.Draw(Format::Number(mass), bright);
//
//			// Truncate the list if there is not enough space.
//			if(table.GetRowBounds().Bottom() >= endY)
//			{
//				hasSpace = false;
//				break;
//			}
//		}
//		table.DrawGap(10.);
//	}
//	if(cargo.HasMissionCargo() && hasSpace)
//	{
//		for(const auto &it : cargo.MissionCargo())
//		{
//			// Capitalize the name of the cargo.
//			table.Draw(Format::Capitalize(it.first->Cargo()), dim);
//			table.Draw(to_string(it.second), bright);
//
//			// Truncate the list if there is not enough space.
//			if(table.GetRowBounds().Bottom() >= endY)
//				break;
//		}
//		table.DrawGap(10.);
//	}
//	if(cargo.Passengers() && endY >= bounds.Top())
//	{
//		table.DrawAt(Point(bounds.Left(), endY) + Point(10., 8.));
//		table.Draw("passengers:", dim);
//		table.Draw(to_string(cargo.Passengers()), bright);
//	}
//}



void SuitInfoPanel::DrawLine(const Point &from, const Point &to, const Color &color) const
{
	Color black(0.f, 1.f);
	Point mid(to.X(), from.Y());
	
	LineShader::Draw(from, mid, 3.5f, black);
	LineShader::Draw(mid, to, 3.5f, black);
	LineShader::Draw(from, mid, 1.5f, color);
	LineShader::Draw(mid, to, 1.5f, color);
}



bool SuitInfoPanel::Hover(const Point &point)
{
	if(suitIt == player.Suits().end())
		return true;

	hoverPoint = point;
	
	hoverIndex = -1;
//	const vector<Hardpoint> &weapons = (**suitIt).Weapons();
//	bool dragIsTurret = (draggingIndex >= 0 && weapons[draggingIndex].IsTurret());
//	for(const auto &zone : zones)
//	{
//		bool isTurret = weapons[zone.Value()].IsTurret();
//		if(zone.Contains(hoverPoint) && (draggingIndex == -1 || isTurret == dragIsTurret))
//			hoverIndex = zone.Value();
//	}
//
	return true;
}



void SuitInfoPanel::Rename(const string &name)
{
	if(suitIt != player.Suits().end() && !name.empty())
	{
		player.RenameSuit(suitIt->get(), name);
		UpdateInfo();
	}
}



bool SuitInfoPanel::CanDump() const
{
	if(canEdit || suitIt == player.Suits().end())
		return false;
	
//	CargoHold &cargo = (*suitIt)->Cargo();
//	return (selectedPlunder && cargo.Get(selectedPlunder) > 0) || cargo.CommoditiesSize() || cargo.BodymodsSize();
    return false;
}



//void SuitInfoPanel::Dump()
//{
//	if(!CanDump())
//		return;
//	
//	CargoHold &cargo = (*suitIt)->Cargo();
//	int commodities = (*suitIt)->Cargo().CommoditiesSize();
//	int amount = cargo.Get(selectedCommodity);
//	int plunderAmount = cargo.Get(selectedPlunder);
//	int64_t loss = 0;
//	if(amount)
//	{
//		int64_t basis = player.GetBasis(selectedCommodity, amount);
//		loss += basis;
//		player.AdjustBasis(selectedCommodity, -basis);
//		(*suitIt)->Jettison(selectedCommodity, amount);
//	}
//	else if(plunderAmount > 0)
//	{
//		loss += plunderAmount * selectedPlunder->Cost();
//		(*suitIt)->Jettison(selectedPlunder, plunderAmount);
//	}
//	else if(commodities)
//	{
//		for(const auto &it : cargo.Commodities())
//		{
//			int64_t basis = player.GetBasis(it.first, it.second);
//			loss += basis;
//			player.AdjustBasis(it.first, -basis);
//			(*suitIt)->Jettison(it.first, it.second);
//		}
//	}
//	else
//	{
//		for(const auto &it : cargo.Bodymods())
//		{
//			loss += it.first->Cost() * max(0, it.second);
//			(*suitIt)->Jettison(it.first, it.second);
//		}
//	}
//	selectedCommodity.clear();
//	selectedPlunder = nullptr;
//	
//	info.Update(**suitIt, player.FleetDepreciation(), player.GetDate().DaysSinceEpoch());
//	if(loss)
//		Messages::Add("You jettisoned " + Format::Credits(loss) + " credits worth of cargo.");
//}



//void SuitInfoPanel::DumpPlunder(int count)
//{
//	int64_t loss = 0;
//	count = min(count, (*suitIt)->Cargo().Get(selectedPlunder));
//	if(count > 0)
//	{
//		loss += count * selectedPlunder->Cost();
//		(*suitIt)->Jettison(selectedPlunder, count);
//		info.Update(**suitIt, player.FleetDepreciation(), player.GetDate().DaysSinceEpoch());
//		
//		if(loss)
//			Messages::Add("You jettisoned " + Format::Credits(loss) + " credits worth of cargo.");
//	}
//}
//
//
//
//void SuitInfoPanel::DumpCommodities(int count)
//{
//	int64_t loss = 0;
//	count = min(count, (*suitIt)->Cargo().Get(selectedCommodity));
//	if(count > 0)
//	{
//		int64_t basis = player.GetBasis(selectedCommodity, count);
//		loss += basis;
//		player.AdjustBasis(selectedCommodity, -basis);
//		(*suitIt)->Jettison(selectedCommodity, count);
//		info.Update(**suitIt, player.FleetDepreciation(), player.GetDate().DaysSinceEpoch());
//		
//		if(loss)
//			Messages::Add("You jettisoned " + Format::Credits(loss) + " credits worth of cargo.");
//	}
//}



void SuitInfoPanel::Disown()
{
	// Make sure a suit really is selected.
	if(suitIt == player.Suits().end() || suitIt->get() == player.Flagsuit())
		return;
	
	// Because you can never disown your flagsuit, the player's suit list will
	// never become empty as a result of disowning a suit.
	const Suit *suit = suitIt->get();
	if(suitIt != player.Suits().begin())
		--suitIt;
	
	player.DisownSuit(suit);
	UpdateInfo();
}
