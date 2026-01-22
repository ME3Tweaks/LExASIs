#include "Autoload/ExtraContent.hpp"

// ExtraContentHUD implementation
// ======================================================================

void ExtraContentHUD::drawColumn_(const wchar_t* name, TArray<FString>* arr)
{
    canvas_->SetPos(start_.X + columnSize_ * column_, start_.Y + 50.f + (rowSize_ * row_));
    canvas_->SetDrawColor(UNPACK_BGRA(colorSubheader_));
    canvas_->DrawText(FString{ const_cast<wchar_t*>(name) }, 1, 0.8f, 0.8f, nullptr);
    canvas_->SetDrawColor(UNPACK_BGRA(colorText_));
    for (UINT i = 0; i < arr->Count(); i++)
    {
        canvas_->SetPos(start_.X + columnSize_ * column_, start_.Y + 70.f + (rowSize_ * row_) + (i * 10.f));
        canvas_->DrawText((*arr)(i), 1, 0.8f, 0.8f, nullptr);
    }
    column_++;
}

void ExtraContentHUD::drawColumn_(const wchar_t* name, TArray<UPackage*>* arr)
{
    canvas_->SetPos(start_.X + columnSize_ * column_, start_.Y + 50.f + (rowSize_ * row_));
    canvas_->SetDrawColor(UNPACK_BGRA(colorSubheader_));
    canvas_->DrawText(FString{ const_cast<wchar_t*>(name) }, 1, 0.8f, 0.8f, nullptr);
    canvas_->SetDrawColor(UNPACK_BGRA(colorText_));
    for (UINT i = 0; i < arr->Count(); i++)
    {
        auto object = (*arr)(i);
        wchar_t objectName[512];
        swprintf_s(objectName, 512, L"%S %S", (object->Class ? object->Class->Name.GetName() : "WTF"), object->Name.GetName());

        canvas_->SetPos(start_.X + columnSize_ * column_, start_.Y + 70.f + (rowSize_ * row_) + (i * 10.f));
        canvas_->DrawText(objectName, 1, 0.8f, 0.8f, nullptr);
    }
    column_++;
}

void ExtraContentHUD::drawIdleBackground_()
{
    canvas_->SetPos(0.f, 0.f);
    canvas_->SetDrawColor(0x7F, 0, 0, 0x3F);
    canvas_->DrawRect(size_.X, size_.Y, canvas_->DefaultTexture);
}

void ExtraContentHUD::drawIdleHeader_()
{
    canvas_->SetDrawColor(0xFF, 0x00, 0x00, 0xFF);

    canvas_->SetPos(start_.X, start_.Y);
    canvas_->DrawText(FString{ L"Autoload.ini profile - IDLE" }, 1, 1.f, 1.f, nullptr);

    canvas_->SetPos(size_.X - start_.X, start_.Y);
    canvas_->DrawTextRA(FString{ L"ASI built " __DATE__ " " __TIME__ }, 1);

    canvas_->Draw2DLine(
        start_.X, start_.Y + 30.f,
        (float)canvas_->SizeX - start_.X, start_.Y + 30.f,
        FColor{ 0x00, 0x00, 0xFF, 0xFF });

}

void ExtraContentHUD::drawBackground_()
{
    canvas_->SetPos(0.f, 0.f);
    canvas_->SetDrawColor(0, 0, 0, 0x7F);
    canvas_->DrawRect(size_.X, size_.Y, canvas_->DefaultTexture);
}

void ExtraContentHUD::drawHeader_()
{
    canvas_->SetDrawColor(UNPACK_BGRA(colorHeader_));

    canvas_->SetPos(start_.X, start_.Y);
    canvas_->DrawText(FString{ L"Autoload.ini profile" }, 1, 1.f, 1.f, nullptr);

    canvas_->SetPos(size_.X - start_.X, start_.Y);
    canvas_->DrawTextRA(FString{ L"ASI built " __DATE__ " " __TIME__ }, 1);

    canvas_->Draw2DLine(
        start_.X, start_.Y + 30.f,
        (float)canvas_->SizeX - start_.X, start_.Y + 30.f,
        colorHeader_);

}

void ExtraContentHUD::drawTopRow_()
{
    column_ = 0;
    row_ = 0;

    drawColumn_(L"2DAs", &extraContent_->Package2DAs);
    drawColumn_(L"GlobalTlks", &extraContent_->GlobalTlks);
    drawColumn_(L"PlotManagers", &extraContent_->PlotManagers);
    drawColumn_(L"StateTransitionMaps", &extraContent_->StateTransitionMaps);
    drawColumn_(L"ConsequenceMaps", &extraContent_->ConsequenceMaps);
}

void ExtraContentHUD::drawBottomRow_()
{
    column_ = 0;
    row_ = 1;

    drawColumn_(L"OutcomeMaps", &extraContent_->OutcomeMaps);
    drawColumn_(L"QuestMaps", &extraContent_->QuestMaps);
    drawColumn_(L"DataCodexMaps", &extraContent_->DataCodexMaps);
    drawColumn_(L"BioAutoConditionals", &extraContent_->BioAutoConditionals);
    drawColumn_(L"LoadedPackages", & extraContent_->LoadedPackages);
}

ExtraContentHUD::ExtraContentHUD(bool drawInitially)
    : extraContent_{ nullptr }
    , draw_{ drawInitially }
    , start_{ 40.f, 40.f }
    , size_{ 0.f, 0.f }
    , canvas_{ nullptr }
    , columnSize_{ 0.f }, rowSize_{ 0.f }
    , column_{ 0 }, row_{ 0 }
    , colorHeader_{ 0xFF , 0xFF , 0x00 , 0xFF }
    , colorSubheader_{ 0x00, 0xFF , 0x00 , 0xFF }
    , colorText_{ 0x00 , 0xFF , 0xFF , 0xFF }
{

}

void ExtraContentHUD::Update(UCanvas* hudCanvas, ExtraContent* extraContent)
{
    extraContent_ = extraContent;
    canvas_ = hudCanvas;
    size_ = FVector2D{ (float)canvas_->SizeX, (float)canvas_->SizeY };

    column_ = 0;
    row_ = 0;
    columnSize_ = (size_.X - start_.X * 2 - 0.f) / columnCount_ - 10;
    rowSize_ = (size_.Y - start_.Y * 2 - 50.f) / rowCount_ - 0;
}

void ExtraContentHUD::Draw()
{
    if (!draw_)
    {
        return;
    }

    if (!extraContent_)
    {
        drawIdleBackground_();
        drawIdleHeader_();
        return;
    }

    // Assuming that extraContent_ is not null from now on.

    drawBackground_();
    drawHeader_();

    canvas_->SetDrawColor(UNPACK_BGRA(colorText_));

    drawTopRow_();
    drawBottomRow_();
}

void ExtraContentHUD::SetVisible(bool draw) 
{ 
    draw_ = draw; 
}

bool ExtraContentHUD::Visible() const noexcept 
{ 
    return draw_; 
}

ExtraContentHUD* ECHUD = nullptr;