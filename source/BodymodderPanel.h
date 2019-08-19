/* BodymodderPanel.h
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef BODYMODDER_PANEL_H_
#define BODYMODDER_PANEL_H_

#include "ShopPanel.h"

#include "Sale.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class Bodymod;
class PlayerInfo;
class Point;
class Suit;



// Class representing the Bodymodder UI panel, which allows you to buy new
// bodymods to install in your suit or to sell the ones you own. Any bodymod you
// sell is available to be bought again until you close this panel, even if it
// is not normally sold here. You can also directly install any bodymod that you
// have plundered from another suit and are storing in your cargo bay. This
// panel makes an attempt to ensure that you do not leave with a suit that is
// configured in such a way that it cannot fly (e.g. no engines or steering).
class BodymodderPanel : public ShopPanel {
public:
	explicit BodymodderPanel(PlayerInfo &player);
	
	virtual void Step() override;
	
	
protected:
	virtual int TileSize() const override;
	virtual int DrawPlayerSuitInfo(const Point &point) override;
	virtual bool HasItem(const std::string &name) const override;
	virtual void DrawItem(const std::string &name, const Point &point, int scrollY) override;
	virtual int DividerOffset() const override;
	virtual int DetailWidth() const override;
	virtual int DrawDetails(const Point &center) override;
	virtual bool CanBuy() const override;
	virtual void Buy(bool fromCargo = false) override;
	virtual void FailBuy() const override;
	virtual bool CanSell(bool toCargo = false) const override;
	virtual void Sell(bool toCargo = false) override;
	virtual void FailSell(bool toCargo = false) const override;
	virtual bool ShouldHighlight(const Suit *suit) override;
	virtual void DrawKey() override;
	virtual void ToggleForSale() override;
	virtual void ToggleCargo() override;
	
	
private:
	static bool SuitCanBuy(const Suit *suit, const Bodymod *bodymod);
	static bool SuitCanSell(const Suit *suit, const Bodymod *bodymod);
	static void DrawBodymod(const Bodymod &bodymod, const Point &center, bool isSelected, bool isOwned);
	bool HasMapped(int mapSize) const;
	bool IsLicense(const std::string &name) const;
	bool HasLicense(const std::string &name) const;
	std::string LicenseName(const std::string &name) const;
	void CheckRefill();
	void Refill();
	// Shared code for reducing the selected suits to those that have the
	// same quantity of the selected bodymod.
	const std::vector<Suit *> GetSuitsToBodymod(bool isBuy = false) const;
	
private:
	// Record whether we've checked if the player needs ammo refilled.
	bool checkedRefill = false;
	// Allow toggling whether bodymods that are for sale are shown. If turned
	// off, only bodymods in the currently selected suits are shown.
	bool showForSale = true;
	// Remember what suits are selected if the player switches to cargo.
	Suit *previousSuit = nullptr;
	std::set<Suit *> previousSuits;
	
	Sale<Bodymod> bodymodder;
};


#endif
