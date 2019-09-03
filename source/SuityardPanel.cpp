/* SuityardPanel.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "SuityardPanel.h"

#include "Color.h"
#include "Dialog.h"
#include "Font.h"
#include "FontSet.h"
#include "Format.h"
#include "GameData.h"
#include "Phrase.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Point.h"
#include "Screen.h"
#include "Suit.h"
#include "SpriteSet.h"
#include "SpriteShader.h"
#include "UI.h"

class System;

using namespace std;

namespace {
	// The name entry dialog should include a "Random" button to choose a random
	// name using the civilian suit name generator.
	class NameDialog : public Dialog {
	public:
		NameDialog(SuityardPanel *panel, void (SuityardPanel::*fun)(const string &), const string &message)
			: Dialog(panel, fun, message) {}
		
		virtual void Draw() override
		{
			Dialog::Draw();
			
			randomPos = cancelPos - Point(80., 0.);
			SpriteShader::Draw(SpriteSet::Get("ui/dialog cancel"), randomPos);

			const Font &font = FontSet::Get(14);
			static const string label = "Random";
			Point labelPos = randomPos - .5 * Point(font.Width(label), font.Height());
			font.Draw(label, labelPos, *GameData::Colors().Get("medium"));
		}
		
	protected:
		virtual bool Click(int x, int y, int clicks) override
		{
			Point off = Point(x, y) - randomPos;
			if(fabs(off.X()) < 40. && fabs(off.Y()) < 20.)
			{
				input = GameData::Phrases().Get("civilian")->Get();
				return true;
			}
			return Dialog::Click(x, y, clicks);
		}
		
	private:
		Point randomPos;
	};
}



SuityardPanel::SuityardPanel(PlayerInfo &player)
	: ShopPanel(player, "suityard"), modifier(0)
{
	for(const auto &it : GameData::Suits())
		catalog[it.second.Attributes().Category()].insert(it.first);
	
	if(player.GetPlanet())
		suityard = player.GetPlanet()->Suityard();
}



int SuityardPanel::TileSize() const
{
	return SUIT_SIZE;
}


int SuityardPanel::DrawPlayerShipInfo(const Point &point)
{
	shipInfo.Update(*playerShip, player.FleetDepreciation(), day);
	shipInfo.DrawAttributes(point);

	return shipInfo.AttributesHeight();
}



int SuityardPanel::DrawPlayerSuitInfo(const Point &point)
{
	suitInfo.Update(*playerSuit, player.FleetDepreciation(), player.GetDate().DaysSinceEpoch());
	suitInfo.DrawSale(point);
	suitInfo.DrawAttributes(point + Point(0, suitInfo.SaleHeight()));
	
	return suitInfo.SaleHeight() + suitInfo.AttributesHeight();
}



bool SuityardPanel::HasItem(const string &name) const
{
	const Suit *suit = GameData::Suits().Get(name);
	return suityard.Has(suit);
}



void SuityardPanel::DrawItem(const string &name, const Point &point, int scrollY)
{
	const Suit *suit = GameData::Suits().Get(name);
	zones.emplace_back(point, Point(SUIT_SIZE, SUIT_SIZE), suit, scrollY);
	if(point.Y() + SUIT_SIZE / 2 < Screen::Top() || point.Y() - SUIT_SIZE / 2 > Screen::Bottom())
		return;
	
	DrawSuit(*suit, point, suit == selectedSuit);
}



int SuityardPanel::DividerOffset() const
{
	return 121;
}



int SuityardPanel::DetailWidth() const
{
	return 3 * suitInfo.PanelWidth();
}



int SuityardPanel::DrawDetails(const Point &center)
{
	suitInfo.Update(*selectedSuit, player.StockDepreciation(), player.GetDate().DaysSinceEpoch());
	Point offset(suitInfo.PanelWidth(), 0.);
	
	suitInfo.DrawDescription(center - offset * 1.5);
	suitInfo.DrawAttributes(center - offset * .5);
	suitInfo.DrawBodymods(center + offset * .5);
	
	return suitInfo.MaximumHeight();
}



bool SuityardPanel::CanBuy() const
{
	if(!selectedSuit)
		return false;
	
	int64_t cost = player.StockDepreciation().Value(*selectedSuit, day);
	
	// Check that the player has any necessary licenses.
	int64_t licenseCost = LicenseCost(&selectedSuit->Attributes());
	if(licenseCost < 0)
		return false;
	cost += licenseCost;
	
	return (player.Accounts().Credits() >= cost);
}



void SuityardPanel::Buy(bool fromCargo)
{
	int64_t licenseCost = LicenseCost(&selectedSuit->Attributes());
	if(licenseCost < 0)
		return;
	
	modifier = Modifier();
	string message;
	if(licenseCost)
		message = "Note: you will need to pay " + Format::Credits(licenseCost)
			+ " credits for the licenses required to operate this suit, in addition to its cost."
			" If that is okay with you, go ahead and enter a name for your brand new ";
	else
		message = "Enter a name for your brand new ";
	
	if(modifier == 1)
		message += selectedSuit->ModelName() + "! (Or leave it blank to use a randomly chosen name.)";
	else
		message += selectedSuit->PluralModelName() + "! (Or leave it blank to use randomly chosen names.)";
	
	GetUI()->Push(new NameDialog(this, &SuityardPanel::BuySuit, message));
}



void SuityardPanel::FailBuy() const
{
	if(!selectedSuit)
		return;
	
	int64_t cost = player.StockDepreciation().Value(*selectedSuit, day);
	
	// Check that the player has any necessary licenses.
	int64_t licenseCost = LicenseCost(&selectedSuit->Attributes());
	if(licenseCost < 0)
	{
		GetUI()->Push(new Dialog("Buying this suit requires a special license. "
			"You will probably need to complete some sort of mission to get one."));
		return;
	}
	
	cost += licenseCost;
	if(player.Accounts().Credits() < cost)
	{
		for(const auto &it : player.Suits())
			cost -= player.FleetDepreciation().Value(*it, day);
		if(player.Accounts().Credits() < cost)
			GetUI()->Push(new Dialog("You do not have enough credits to buy this suit. "
				"Consider checking if the bank will offer you a loan."));
		else
		{
			string suit = (player.Suits().size() == 1) ? "your current suit" : "one of your suits";
			GetUI()->Push(new Dialog("You do not have enough credits to buy this suit. "
				"If you want to buy it, you must sell " + suit + " first."));
		}
		return;
	}
}



bool SuityardPanel::CanSell(bool toCargo) const
{
	return playerSuit;
}



void SuityardPanel::Sell(bool toCargo)
{
	static const int MAX_LIST = 20;
	static const int MAX_NAME_WIDTH = 250 - 30;
	
	int count = playerSuits.size();
	int initialCount = count;
	string message = "Sell the ";
	const Font &font = FontSet::Get(14);
	if(count == 1)
		message += playerSuit->Name();
	else if(count <= MAX_LIST)
	{
		auto it = playerSuits.begin();
		message += (*it++)->Name();
		--count;
		
		if(count == 1)
			message += " and ";
		else
		{
			while(count-- > 1)
				message += ",\n" + font.TruncateMiddle((*it++)->Name(), MAX_NAME_WIDTH);
			message += ",\nand ";
		}
		message += (*it)->Name();
	}
	else
	{
		auto it = playerSuits.begin();
		message += (*it++)->Name() + ",\n";
		for(int i = 1; i < MAX_LIST - 1; ++i)
			message += font.TruncateMiddle((*it++)->Name(), MAX_NAME_WIDTH) + ",\n";
		
		message += "and " + to_string(count - (MAX_LIST - 1)) + " other suits";
	}
	// To allow calculating the sale price of all the suits in the list,
	// temporarily copy into a shared_ptr vector:
	vector<shared_ptr<Suit>> toSell;
	for(const auto &it : playerSuits)
		toSell.push_back(it->shared_from_this());
	int64_t total = player.FleetDepreciation().Value(toSell, day);
	
	message += ((initialCount > 2) ? "\nfor " : " for ") + Format::Credits(total) + " credits?";
	GetUI()->Push(new Dialog(this, &SuityardPanel::SellSuit, message));
}



bool SuityardPanel::CanSellMultiple() const
{
	return false;
}



void SuityardPanel::BuySuit(const string &name)
{
	int64_t licenseCost = LicenseCost(&selectedSuit->Attributes());
	if(licenseCost)
	{
		player.Accounts().AddCredits(-licenseCost);
		for(const string &licenseName : selectedSuit->Attributes().Licenses())
			if(player.GetCondition("license: " + licenseName) <= 0)
				player.Conditions()["license: " + licenseName] = true;
	}
	
	for(int i = 1; i <= modifier; ++i)
	{
		// If no name is given, choose a random name. Otherwise, if buying
		// multiple suits, append a number to the given suit name.
		string suitName = name;
		if(name.empty())
			suitName = GameData::Phrases().Get("civilian")->Get();
		else if(modifier > 1)
			suitName += " " + to_string(i);
		
		player.BuySuit(selectedSuit, suitName);
	}
	
	playerSuit = &*player.Suits().back();
	playerSuits.clear();
	playerSuits.insert(playerSuit);
}



void SuityardPanel::SellSuit()
{
	for(Suit *suit : playerSuits)
		player.SellSuit(suit);
	playerSuits.clear();
	playerSuit = nullptr;
	for(const shared_ptr<Suit> &suit : player.Suits()) {
        playerSuit = suit.get();
        break;
	}

//		if(suit->GetSystem() == player.GetSystem() && !suit->IsDisabled())
//		{
//			playerSuit = suit.get();
//			break;
//		}
	if(playerSuit)
		playerSuits.insert(playerSuit);
	player.UpdateCargoCapacities();
}
