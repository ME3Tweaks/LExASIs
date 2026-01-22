#pragma once
#include <Windows.h>
#include <LESDK/Headers.hpp>
#include "Common/Base.hpp"

// Class / object mixins.
// ======================================================================

class NonCopyMovable
{
public:
    NonCopyMovable() = default;
    NonCopyMovable(const NonCopyMovable& other) = delete;
    NonCopyMovable(NonCopyMovable&& other) = delete;
    NonCopyMovable& operator=(const NonCopyMovable& other) = delete;
    NonCopyMovable& operator=(NonCopyMovable&& other) = delete;
};

class NonConstructible
{
public:
    NonConstructible() = delete;
    ~NonConstructible() = delete;
};


/// <summary>
/// Class used by BioWare instead of UE's
/// built-in DLC subsystem to load additional content.
/// </summary>
class ExtraContent final
{
public:
    TArray<FString> Package2DAs;
    TArray<FString> GlobalTlks;
    TArray<FString> PlotManagers;
    TArray<FString> GlobalPackages;
    TArray<FString> StateTransitionMaps;
    TArray<FString> ConsequenceMaps;
    TArray<FString> OutcomeMaps;
    TArray<FString> QuestMaps;
    TArray<FString> DataCodexMaps;
    TArray<FString> BioAutoConditionals;
    TArray<UPackage*> LoadedPackages; // Populated after InstallDownloadableContent() has run (which contains ProcessIni)

    // Probably has some other stuff here too?

    // For sure from decomp:
    // ArmorMale
    // ArmorFemale
    // HeadGearMale
    // HeadGearFemale
};


#define UNPACK_BGRA(STRUCT) STRUCT.R, STRUCT.G, STRUCT.B, STRUCT.A

/// <summary>
/// A small class to draw debug HUD
/// for enumerating all of the loaded "DLC" content.
/// </summary>
class ExtraContentHUD final
    : private NonCopyMovable
{
private:
    ExtraContent* extraContent_;
    UCanvas* canvas_;
    bool draw_;

    FVector2D size_;
    FVector2D start_;

    FColor colorHeader_;
    FColor colorSubheader_;
    FColor colorText_;

    const int columnCount_ = 5;
    const int rowCount_ = 2;

    float columnSize_;
    float rowSize_;

    int column_;
    int row_;

    // Functions to draw individual arrays
    // ======================================================================

    void drawColumn_(const wchar_t* name, TArray<FString>* arr);
    void drawColumn_(const wchar_t* name, TArray<UPackage*>* arr);

    // Functions to draw the bigger elements of this HUD
    // ======================================================================

    void drawIdleBackground_();
    void drawIdleHeader_();
    void drawBackground_();
    void drawHeader_();
    void drawTopRow_();
    void drawBottomRow_();

public:
    ExtraContentHUD(bool drawInitially);

    void Update(UCanvas* hudCanvas, ExtraContent* extraContent);
    void Draw();
    void SetVisible(bool draw);
    bool Visible() const noexcept;
};

// The Extra Content HUD
extern ExtraContentHUD* ECHUD;