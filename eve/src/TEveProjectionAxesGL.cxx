// @(#)root/eve:$Id$
// Author: Matevz Tadel 2007

/*************************************************************************
 * Copyright (C) 1995-2007, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "TEveProjectionAxesGL.h"
#include "TEveProjectionAxes.h"
#include "TEveProjectionManager.h"

#include "TGLRnrCtx.h"
#include "TGLIncludes.h" 
#include "TFTGLManager.h"
#include "FTFont.h"
#include "TMath.h"

//______________________________________________________________________________
// OpenGL renderer class for TEveProjectionAxes.
//

ClassImp(TEveProjectionAxesGL);

//______________________________________________________________________________
// TEveProjectionAxesGL
//
// GL-renderer for TEveProjectionAxes.


//______________________________________________________________________________
TEveProjectionAxesGL::TEveProjectionAxesGL() :
   TEveTextGL(),

   fRange(300),
   fLabelSize(0.02),
   fLabelOff(0.5),
   fTMSize(0.02),

   fAxesModel(0),
   fProjection(0)
{
   // Constructor.

   fDLCache = kFALSE; // Disable display list.
}

/******************************************************************************/

//______________________________________________________________________________
Bool_t TEveProjectionAxesGL::SetModel(TObject* obj, const Option_t* /*opt*/)
{
   // Set model object.
   // Virtual from TGLObject.

   if (SetModelCheckClass(obj, TEveProjectionAxes::Class())) {
      fAxesModel = dynamic_cast<TEveProjectionAxes*>(obj);
      if( fAxesModel->GetManager() == 0)
         return kFALSE;
      return kTRUE;
   }
   return kFALSE;
}

//______________________________________________________________________________
void TEveProjectionAxesGL::SetBBox()
{
   // Fill the bounding-box data of the logical-shape.
   // Virtual from TGLObject.

   // !! This ok if master sub-classed from TAttBBox
   SetAxisAlignedBBox(((TEveProjectionAxes*)fExternalObj)->AssertBBox());
}
/******************************************************************************/

//______________________________________________________________________________
const char* TEveProjectionAxesGL::GetText(Float_t x) const
{
   // Get formatted text.

   using  namespace TMath;
   if (Abs(x) > 1000) {
      Float_t v = 10*TMath::Nint(x/10.0f);
      return Form("%.0f", v);
   } else if(Abs(x) > 100) {
      Float_t v = TMath::Nint(x);
      return Form("%.0f", v);
   } else if (Abs(x) > 10)
      return Form("%.1f", x);
   else if ( Abs(x) > 1 )
      return Form("%.2f", x);
   else // zero
      return Form("%.1f", x);
}

//______________________________________________________________________________
void TEveProjectionAxesGL::RenderText(const char* txt, Float_t x, Float_t y) const
{
   // Render FTFont at given location.

   if (fMode < TFTGLManager::kTexture) {
      glRasterPos3f(0, 0, 0);
      glBitmap(0, 0, 0, 0, x, y, 0);
      fFont->Render(txt);
   } else {
      glPushMatrix();
      glTranslatef(x, y, 0);
      fFont->Render(txt);
      glPopMatrix();
   }
}

/******************************************************************************/

//______________________________________________________________________________
void TEveProjectionAxesGL::SetRange(Float_t pos, Int_t ax) const
{
   // Set range from bounding box.
  
   using namespace TMath;

   Float_t limit =  fProjection->GetLimit(ax, pos > 0 ? kTRUE: kFALSE);
   if (fProjection->GetDistortion() > 0.001 && Abs(pos) > Abs(limit *0.97))
   {
      pos = limit*0.95;
   }
   
   fTMList.push_back(TM_t(pos, fProjection->GetValForScreenPos(ax, pos)));
}

/******************************************************************************/

//______________________________________________________________________________
void TEveProjectionAxesGL::DrawTickMarks(Float_t y) const
{
   // Draw tick-marks on the current axis.

   glBegin(GL_LINES);
   for (std::list<TM_t>::iterator it = fTMList.begin(); it!= fTMList.end(); ++it)
   {
      glVertex2f((*it).first, 0);
      glVertex2f((*it).first, y);
   }
   glEnd();
}

//______________________________________________________________________________
void TEveProjectionAxesGL::DrawHInfo() const
{
   // Draw labels on horizontal axis.

   Float_t tmH = fTMSize*fRange;
   DrawTickMarks(-tmH);

   Float_t off = tmH + fLabelOff*tmH;
   Float_t llx, lly, llz, urx, ury, urz; 
   const char* txt;
   for (std::list<TM_t>::iterator it = fTMList.begin(); it!= fTMList.end(); ++it)
   {
      glPushMatrix();
      glTranslatef((*it).first, -off, 0);
      txt = GetText((*it).second);
      fFont->BBox(txt, llx, lly, llz, urx, ury, urz);
      Float_t xd = -0.5f*(urx+llx);
      if (txt[0] == '-')
         xd -= 0.5f * (urx-llx) / strlen(txt);
      RenderText(txt, xd, -ury);
      glPopMatrix();
   }

   fTMList.clear();
}

//______________________________________________________________________________
void TEveProjectionAxesGL::DrawVInfo() const
{
   // Draw labels on vertical axis.

   Float_t tmH = fTMSize*fRange;

   glPushMatrix();
   glRotatef(90, 0, 0, 1);
   DrawTickMarks(tmH);
   glPopMatrix();

   Float_t off = fLabelOff*tmH + tmH;
   Float_t llx, lly, llz, urx, ury, urz;
   const char* txt;
   for (std::list<TM_t>::iterator it = fTMList.begin(); it!= fTMList.end(); ++it)
   {
      glPushMatrix();
      glTranslatef(-off, (*it).first, 0);
      txt = GetText((*it).second);
      fFont->BBox(txt, llx, lly, llz, urx, ury, urz);
      RenderText(txt, -urx, -0.38f*(ury+lly));
      glPopMatrix();
   }

   fTMList.clear();
}

/******************************************************************************/

//______________________________________________________________________________
void TEveProjectionAxesGL::SplitInterval(Int_t ax) const
{
   // Build a list of labels and their positions.

   if (fAxesModel->GetSplitLevel())
   {
      if (fAxesModel->GetSplitMode() == TEveProjectionAxes::kValue) 
      {
         SplitIntervalByVal(fTMList.front().second, fTMList.back().second, ax, 0);
      }
      else if (fAxesModel->GetSplitMode() == TEveProjectionAxes::kPosition)
      {
         SplitIntervalByPos(fTMList.front().first, fTMList.back().first, ax, 0);
      }
   }
}

//______________________________________________________________________________
void TEveProjectionAxesGL::SplitIntervalByPos(Float_t minp, Float_t maxp, Int_t ax, Int_t level) const
{
   // Add tick-mark and label with position in the middle of given interval.

   Float_t p = (minp+maxp)*0.5;
   Float_t v = fProjection->GetValForScreenPos(ax, p);
   fTMList.push_back(TM_t(p, v));

   level++;
   if (level<fAxesModel->GetSplitLevel())
   {
      SplitIntervalByPos(minp, p , ax, level);
      SplitIntervalByPos(p, maxp, ax, level);
   }
}

//______________________________________________________________________________
void TEveProjectionAxesGL::SplitIntervalByVal(Float_t minv, Float_t maxv, Int_t ax, Int_t level) const
{
   // Add tick-mark and label with value in the middle of given interval.

   Float_t v = (minv+maxv)*0.5;
   Float_t p = fProjection->GetScreenVal(ax, v);
   fTMList.push_back(TM_t(p, v));

   level++;
   if (level<fAxesModel->GetSplitLevel())
   {
      SplitIntervalByVal(minv, v , ax, level);
      SplitIntervalByVal(v, maxv, ax, level);
   }
}

/******************************************************************************/

//______________________________________________________________________________
void TEveProjectionAxesGL::DirectDraw(TGLRnrCtx& rnrCtx) const
{
   // Actual rendering code.
   // Virtual from TGLLogicalShape.

   fProjection = fAxesModel->GetManager()->GetProjection();

   Float_t* bbox = fAxesModel->GetBBox();
   fRange = bbox[1] - bbox[0];
   TEveVector zeroPos;
   fProjection->ProjectVector(zeroPos);
    
   SetModelFont(fAxesModel, rnrCtx);
   TFTGLManager::PreRender(fMode);
   {  
      glPushMatrix();
      glTranslatef(0, bbox[2], 0);
      // left
      fTMList.push_back(TM_t(zeroPos.fX, 0));
      SetRange(bbox[0], 0); SplitInterval(0);
      DrawHInfo();
      // right, skip middle
      fTMList.push_back(TM_t(zeroPos.fX, 0));
      SetRange(bbox[1], 0); SplitInterval(0); fTMList.pop_front();
      DrawHInfo();
      glPopMatrix();
   }
   {
      glPushMatrix();
      glTranslatef(bbox[0], 0, 0);
      // labels bottom
      fTMList.push_back(TM_t(zeroPos.fY, 0));
      SetRange(bbox[2], 1); SplitInterval(1);
      DrawVInfo();
      // labels top, skip middle
      fTMList.push_back(TM_t(zeroPos.fY, 0));
      SetRange(bbox[3], 1); SplitInterval(1); fTMList.pop_front();
      DrawVInfo();
      glPopMatrix();
   }

   // axes title
   glPushMatrix();
   glTranslatef(zeroPos.fX, bbox[3]*1.1, 0);
   Float_t llx, lly, llz, urx, ury, urz; 
   fFont->BBox(fAxesModel->GetText(), llx, lly, llz, urx, ury, urz);
   RenderText(fAxesModel->GetText(), -llx, 0);
   glPopMatrix();

   TFTGLManager::PostRender(fMode);

   // axes lines
   glBegin(GL_LINES);
   glVertex3f(bbox[0], bbox[2], 0.);
   glVertex3f(bbox[1], bbox[2], 0.);
   glVertex3f(bbox[0], bbox[2], 0.);
   glVertex3f(bbox[0], bbox[3], 0.);
   glEnd();

   // projection center
   Float_t d = 10;
   if (fAxesModel->GetDrawCenter()) {
      Float_t* c = fProjection->GetProjectedCenter();
      TGLUtil::Color3f(1., 0., 0.);
      glBegin(GL_LINES);
      glVertex3f(c[0] +d, c[1],    c[2]);     glVertex3f(c[0] - d, c[1]   , c[2]);
      glVertex3f(c[0] ,   c[1] +d, c[2]);     glVertex3f(c[0]    , c[1] -d, c[2]);
      glVertex3f(c[0] ,   c[1],    c[2] + d); glVertex3f(c[0]    , c[1]   , c[2] - d);
      glEnd();
   }

   if (fAxesModel->GetDrawOrigin()) {
      TEveVector zero;
      fProjection->ProjectVector(zero);
      TGLUtil::Color3f(1., 1., 1.);
      glBegin(GL_LINES);
      glVertex3f(zero[0] +d, zero[1],    zero[2]);     glVertex3f(zero[0] - d, zero[1]   , zero[2]);
      glVertex3f(zero[0] ,   zero[1] +d, zero[2]);     glVertex3f(zero[0]    , zero[1] -d, zero[2]);
      glVertex3f(zero[0] ,   zero[1],    zero[2] + d); glVertex3f(zero[0]    , zero[1]   , zero[2] - d);
      glEnd();
   }

   fProjection = 0;
}


