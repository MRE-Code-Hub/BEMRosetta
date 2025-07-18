// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2020 - 2022, the BEMRosetta author and contributors
#include <CtrlLib/CtrlLib.h>
#include <Controls4U/Controls4U.h>
#include <ScatterCtrl/ScatterCtrl.h>
#include <SurfaceCanvas/SurfaceCanvas.h>
#include <RasterPlayer/RasterPlayer.h>
#include <TabBar/TabBar.h>
#include <DropGrid/DropGrid.h>

#include <BEMRosetta_cl/BEMRosetta.h>
#include <BEMRosetta_cl/functions.h>

using namespace Upp;

#include "main.h"


void QTFTabDof::Init(MainQTF &par, int posSplitter, int _ib, int _idof) {
	this->ib = _ib;
	this->idof = _idof;
	Add(splitter);
	splitter.Horz(leftsplit.SizePos(), rightsplit.SizePos());
	splitter.SetPos(posSplitter, 0);
	leftsplit.Add(up.array, 0, 0).Add(down.array, 1, 0);
	rightsplit.Add(up.sc, 0, 0).Add(down.sc, 1, 0);
	
	up.sc.Add(up.surf, 0, 0).Add(up.scatter, 0, 1);
	down.sc.Add(down.surf, 0, 0).Add(down.scatter, 0, 1);
	up.sc.WhenWidths = down.sc.WhenWidths = [&](int width, int height, UVector<int> &widths) {
		widths[0] = height;
		widths[1] = max(0, width - height);
	};
	
	up.isUp = true;
	down.isUp = false;
		
	up	.surf.ShowInfo().ShowContextMenu().ShowPropertiesDlg().ShowProcessDlg().SetLeftMargin(50).SetTopMargin(25).SetBottomMargin(50);
	down.surf.ShowInfo().ShowContextMenu().ShowPropertiesDlg().ShowProcessDlg().SetLeftMargin(50).SetTopMargin(25).SetBottomMargin(50);
	up	.surf.LinkedWith(down.surf);
	
	up  .surf.WhenPainter = THISBACK(OnPainter);
	down.surf.WhenPainter = THISBACK(OnPainter);
	up  .surf.WhenDraw    = THISBACK(OnDraw);
	down.surf.WhenDraw    = THISBACK(OnDraw);
	
	up  .surf.WhenMouseClick = [&](Point p, dword keyflags, ScatterCtrl::MouseAction action) {OnClick(p, /*this->idof, */action);};
	down.surf.WhenMouseClick = [&](Point p, dword keyflags, ScatterCtrl::MouseAction action) {OnClick(p, /*this->idof, */action);};
	
	int len = StdFont().GetHeight();
	
	up  .surf.SetMargin(4*len, len, int(2.5*len), 4*len);
	down.surf.SetMargin(4*len, len, int(2.5*len), 4*len);
	
	up.  scatter.SetMargin(6*len, len, len, 4*len).SetTitleFont(SansSerifZ(12)).ShowAllMenus().SetSciExpTop();
	down.scatter.SetMargin(6*len, len, len, 4*len).SetTitleFont(SansSerifZ(12)).ShowAllMenus().SetSciExpTop();		   
	up.scatter.LinkedWith(down.scatter);
	
	parent = &par;
}

Pointf &QTFTabDof::Pf() {
	return parent->pf;
}

void QTFTabDof::DoClick(Data &data) {
	data.dataPlot.Clear();
	data.scatter.RemoveAllSeries();
	
	String strmag;
	
	if (data.show_ma_ph) {
		if (data.isUp) {
			data.labelY = t_("Magnitude");
			data.ma_ph = t_("ma");
			strmag = t_("mag");
			data.units = idof < 3 ? t_("N/m²") : t_("N m/m²");
		} else {
			data.labelY = t_("Phase");
			data.ma_ph = t_("ph");
			strmag = t_("phase");
			data.units = t_("rad");
		}
	} else {
		if (data.isUp) {
			data.labelY = t_("Real");
			data.ma_ph = t_("re");
			strmag = t_("real");
		} else {
			data.labelY = t_("Imaginary");
			data.ma_ph = t_("im");
			strmag = t_("imag");
		}
		data.units = idof < 3 ? t_("N/m²") : t_("N m/m²");
	}
	data.scatter.SetLabelY(data.labelY);
	data.scatter.SetLabelX(Format(show_w ? t_("ω%s [rad/s]") : t_("T%s [s]"), typec == 'v' ? CharToSubSupScript('y', true) : CharToSubSupScript('x', true)));
	
	String sdiff;
	double avgT = 0;
	for (int i = 0; i < Bem().hydros.size(); ++i) {
		const Hydro &hy = Bem().hydros[i];
		if (!hy.IsLoadedQTF(isSum))
			continue;
		 
		int idh = FindDelta(hy.dt.qhead, FixHeading_0_360(head), 2.);
		if (idh < 0) 
			continue;
		
		VectorXd xAxis = hy.dt.qw;
		if (!show_w) {
			for (double &d : xAxis)
				d = 2*M_PI/d;
			ReverseX(xAxis);
		}
		
		MatrixXd zData = GetMat(hy, data, idh, /*show_w, */!ndim);
		if (zData.size() == 0)
			continue;
		
		UArray<Pointf> &d = data.dataPlot.Add();	
		
		int id = Null;
		if (IsNull(Pf())) {
			double freq = Avg(Last(xAxis), First(xAxis));
			id = FindClosest(xAxis, freq);
			Pf().x = Pf().y = xAxis(id);
		}
		Pointf from, to;
		double a, b;
		
		if (typec == 'h')
			avgT += Pf().y;
		else if (typec == 'v')
			avgT += Pf().x;
		else if (typec == 'd') {
			Diagonal(Pf(), First(xAxis), Last(xAxis), from, to, a, b);
			double wdiff;
			if (IsNull(from))
				sdiff = "-";
			else {
				if (isSum)
					wdiff = from.x + from.y;
				else
					wdiff = from.x - from.y;
				if (!show_w) {
					if (abs(wdiff) < 1E-8)
						wdiff = Null;
					else	
						wdiff = 2*M_PI/wdiff;
				}
				if (!IsNull(wdiff))
					sdiff = FormatDouble(wdiff, 2);
				else
					sdiff = "-";
			}
		} else
			Conjugate(Pf(), First(xAxis), Last(xAxis), from, to, a, b);
		
		if (typec == 'h') {
			for (int iw = 0; iw < xAxis.size(); ++iw) 
				d << Pointf(xAxis(iw), BilinearInterpolate(Pf().y, xAxis(iw), xAxis, xAxis, zData));	// row, col order, to fit with zData
		} else if (typec == 'v') {
			for (int iw = 0; iw < xAxis.size(); ++iw) 
				d << Pointf(xAxis(iw), BilinearInterpolate(xAxis(iw), Pf().x, xAxis, xAxis, zData));
		} else {
			if (!IsNull(from) && !IsNull(to)) {
				for (int iw = 0; iw < xAxis.size(); ++iw) 
					if (Between(xAxis(iw), from.x, to.x)) 
						d << Pointf(xAxis(iw), BilinearInterpolate(a*xAxis(iw) + b, xAxis(iw), xAxis, xAxis, zData));
			}
		}
		int idc = hy.dt.GetId();
		const Upp::Color &color = GetColorId(idc);
		String nameType = Format(t_("QTF %s %s(%s) %d"), data.ma_ph, hy.dt.name, hy.GetCodeStrAbr(), hy.dt.qtftype);
		data.scatter.AddSeries(d).Legend(nameType).Units(data.units).SetMarkColor(color).Stroke(2, color);
		if (!showPoints)
			data.scatter.NoMark();
	}
	if (typec == 'h' || typec == 'v')
		avgT /= Bem().hydros.size();		// Average value
	
	String strw;
	if (typec == 'd')
		strw = Format(t_("Diagonal %s %s"), sdiff, show_w ? "rad/s" : "s");
	else if (typec == 'c')
		strw = t_("Conjugate");
	else 
		strw = Format("%.2f %s", avgT, show_w ? "rad/s" : "s");
	data.scatter.SetTitle(Format(t_("QTF %s %d.%s %s heading %.1f:%.1fº %s"), isSum ? "sum" : "dif", ib+1, BEM::StrDOF(idof), strw, real(head), imag(head), strmag));
	
	if (autoFit) {
		data.scatter.ZoomToFit(true, true);
		if (data.isUp || !data.show_ma_ph) {
			if (fromY0) {
				double yRange = max<double>(0, data.scatter.GetYMin()) + data.scatter.GetYRange();
				data.scatter.SetXYMin(Null, 0).SetRange(Null, yRange);
			}
		} else {
			if (data.show_ma_ph && !data.isUp) {	// Phase
				data.scatter.ZoomToFit(true, false);
				data.scatter.SetXYMin(Null, -M_PI).SetRange(Null, 2*M_PI).SetMajorUnits(Null, 1);
				data.scatter.SetMinUnits(Null, M_PI-3);
			} else if (fromY0) {
				double yRange = max<double>(0, data.scatter.GetYMin()) + data.scatter.GetYRange();
				data.scatter.SetXYMin(Null, 0).SetRange(Null, yRange);
			} 
		}
	}
	data.scatter.Refresh();
}

void QTFTabDof::OnClick(Point p, /*int idof, */ScatterCtrl::MouseAction action) {
	if (action != ScatterCtrl::LEFT_DOWN && action != ScatterCtrl::LEFT_MOVE)
		return;
	
	Pf().x = up.surf.GetRealPosX(p.x);
	Pf().y = up.surf.GetRealPosY(p.y);
	
	up.surf.Refresh();
	down.surf.Refresh();
	
	DoClick(up);
	DoClick(down);
}

char QTFTabDof::GetWhat(const Data &data) {
	if (data.show_ma_ph) {
		if (data.isUp)
			return 'm';
		else
			return 'p';
	} else {
		if (data.isUp)
			return 'r';
		else 
			return 'i';
	}
}
	
double QTFTabDof::GetData(const Hydro &hy, const Data &data, int idh, int ifr1, int ifr2, bool getDim) const {
	return hy.GetQTFVal(ib, idof, idh, ifr1, ifr2, isSum, GetWhat(data), getDim);
}

MatrixXd QTFTabDof::GetMat(const Hydro &hy, const Data &data, int idh, /*bool show_w, */bool getDim) const {
	MatrixXd m = hy.GetQTFMat(ib, idof, idh, isSum, GetWhat(data), getDim);
	if (!show_w)
		ReverseX(m);
	return m;
}
			
void QTFTabDof::UpdateArray(const Hydro &hy, bool show_ma_ph, Data &data, bool opBilinear) {
	data.show_ma_ph = show_ma_ph;
	
	int qtfNf = int(hy.dt.qw.size());

	data.xAxis = hy.dt.qw;
	if (!show_w) {
		for (double &d : data.xAxis)
			d = 2*M_PI/d;
		ReverseX(data.xAxis);
	}
	data.zData = GetMat(hy, data, ih, /*show_w, */!ndim);
	if (data.zData.size() == 0)
		return;

	ArrayCtrl &array = data.array;
	
	array.Reset();
	array.MultiSelect().SetLineCy(EditField::GetStdHeight()).HeaderObject().Absolute();
	array.WhenBar = [&](Bar &menu) {ArrayCtrlWhenBar(menu, array);};

	array.AddColumn(show_w ? t_("ω [rad/s]") : t_("T [s]"), 60);
	for (int c = 0; c < data.xAxis.size(); ++c) {
		array.AddColumn(FDS(data.xAxis(c), 8), 90);
		array.Set(c, 0, FDS(data.xAxis(c), 8));
	}
	
	double mn = data.zData.minCoeff(), mx = data.zData.maxCoeff();
	if (mx == mn)
		mx = Null;
			
	for (int if1 = 0; if1 < qtfNf; ++if1) {
		for (int if2 = 0; if2 < qtfNf; ++if2) {
			double val = data.zData(if1, if2);
			if (IsNull(val)) 
				array.Set(if1, if2+1, "-");
			else {				
				if (show_ma_ph && !data.isUp) 
					array.Set(if1, if2+1, FDS(val, 10, false));	// Showing phase
				else {
					if (IsNull(mx)) 
						array.Set(if1, if2+1, FDS(val, 10, false));
					else {
						double rat = (val - mn)/(mx - mn);
						
						::Color backColor = GetRainbowColor(rat, White(), LtBlue(), 0);
						::Color color = Black();
						if (Grayscale(backColor) < 150)
							color = White();
						
						String str = FDS(val, 10, false);
						
						array.Set(if1, if2+1, AttrText(str).Center().Ink(color).Paper(backColor));
					}
				}
			}
		}
	}
	data.dataSurf.Init(data.zData, data.xAxis, data.xAxis, opBilinear ? TableInterpolate::BILINEAR : TableInterpolate::NO, false);

	data.surf.AddSurf(data.dataSurf);
	data.surf.SetRainbowPaletteTextColor(White);
	data.surf.ZoomToFitZ().ZoomToFit(true, true);
}
	
void QTFTabDof::Load(const Hydro &hy, int _ib, int _ih, int _idof, bool _ndim, bool _show_w, 
				bool show_ma_ph, bool _isSum, bool opBilinear, bool _showPoints, bool _fromY0, 
				bool _autoFit, int posSplitter, bool resetPf) {
	try {
		splitter.SetPos(posSplitter, 0);
		
		const UArray<UArray<UArray<MatrixXcd>>> &qtf = _isSum ? hy.dt.qtfsum : hy.dt.qtfdif;
		if (qtf.size() <= _ib || qtf[_ib].size() <= _ih || qtf[_ib][_ih].size() <= _idof)
			return;
		
		this->isSum = _isSum;
		this->ib = _ib;
		this->ih = _ih;
		this->idof = _idof;
		this->ndim = _ndim;
		this->show_w = _show_w;
		this->head = FixHeading(hy.dt.qhead[ih], Bem().headingType);
		this->showPoints = _showPoints;
		this->fromY0 = _fromY0;
		this->autoFit = _autoFit;
		
		if (resetPf)
			Pf() = Null;
		
		UpdateArray(hy, show_ma_ph, up, opBilinear);
		UpdateArray(hy, show_ma_ph, down, opBilinear);
		DoClick(up/*, idof*/);
		DoClick(down/*, idof*/);
	} catch (Exc e) {
		BEM::PrintError(DeQtfLf(e));
	}	
}	
	
void MainQTF::Init(MainBEM &parent) {
	CtrlLayout(*this);
	
	try {
		_mbm = &parent;
		ArrayCtrl &headQTF = parent.menuPlot.headQTF;
		
		headQTF.Reset();
		headQTF.NoHeader();
		headQTF.AddColumn("", 20);
		headQTF.AddColumn("", 20);
		
		opLine <<= 0;
		
		opQTF  		<< [&] {
			isSumm = opQTF.GetData() == FSUM;
			OnHeadingsSel(&headQTF, false);
		};
		opBilinear  	<< THISBACK(OnSurf);
		opLine 			<< THISBACK2(OnHeadingsSel, &headQTF, true);
		
		headQTF.WhenSel << THISBACK2(OnHeadingsSel, &headQTF, false);
		headQTF.WhenLeftDouble = [&]() {_mbm->menuPlot.butList.WhenAction();};
		tab.WhenSet 	<< THISBACK2(OnHeadingsSel, &headQTF, false);
	} catch (Exc e) {
		BEM::PrintError(DeQtfLf(e));
	}		
}

void MainQTF::OnHeadingsSel(ArrayCtrl *headQTF, bool resetPf) {
	if (isLoading)
		return;
	
	int row = headQTF->GetCursor();
	if (row < 0)
		return;

	Unload(idof);
	
	try {
		WaitCursor wait;
		
		MainBEM &mbm = *_mbm;

		int idxHydro = mbm.GetIndexOneSelected(false);
		if (idxHydro < 0)
			return;
	
		const Hydro &hy = Bem().hydros[idxHydro];
	
		head.real(FixHeading_0_360(headQTF->Get(row, 0)));
		head.imag(FixHeading_0_360(headQTF->Get(row, 1)));
		int ih = hy.dt.FindClosestQTFHead(head);
		head = FixHeading(hy.dt.qhead[ih], Bem().headingType);
		
		bool ndim = mbm.menuPlot.showNdim;
		bool show_w = mbm.menuPlot.opwT == 0;
		bool show_ma_ph = mbm.menuPlot.opMP == 0;
		bool showPoints = mbm.menuPlot.showPoints;
		bool fromY0 = mbm.menuPlot.fromY0;
		bool autoFit = mbm.menuPlot.autoFit;
		bool isSum = opQTF.GetData() == FSUM;
		
		idof = tab.Get();
		if (idof < 0)
			return;
		
		ib = idof/6;
		idof = idof - 6*ib;

		OnSurf();
		dof[idof+6*ib].Load(hy, ib, ih, idof, ndim, show_w, show_ma_ph, isSum, ~opBilinear, showPoints, fromY0, autoFit, posSplitter, resetPf);
		
	} catch (Exc e) {
		BEM::PrintError(DeQtfLf(e));
	}
}

void MainQTF::OnSurf() {
	switch (int(~opLine)) {
	case 0:	dof[idof+6*ib].typec = 'd';	break;
	case 1:	dof[idof+6*ib].typec = 'c';	break;
	case 2:	dof[idof+6*ib].typec = 'h';	break;
	default:dof[idof+6*ib].typec = 'v';
	}
	dof[idof+6*ib].up.dataSurf.SetInterpolate(opBilinear ? TableInterpolate::BILINEAR : TableInterpolate::NO);
	dof[idof+6*ib].up.surf.Refresh();
	dof[idof+6*ib].down.dataSurf.SetInterpolate(opBilinear ? TableInterpolate::BILINEAR : TableInterpolate::NO);
	dof[idof+6*ib].down.surf.Refresh();
}
	
bool MainQTF::Load() {
	try {	
		MainBEM &mbm = *_mbm;
		
		int idxHydro;
		
		{
			TempAssign<bool> _isLoading(isLoading, true);

			tab.Reset();

			//idHydro = mbm.GetIdOneSelected(false);
			//if (idHydro < 0) 
			//	return false;
						
			dof.SetCount(6*Bem().Nb);
			for (int iib = 0; iib < Bem().Nb; ++iib) {
				for (int idf = 0; idf < 6; ++idf) {
					dof[idf + 6*iib].Init(*this, posSplitter, iib, idf);
					tab.Add(dof[idf + 6*iib].SizePos(), Format("%d.%s", iib+1, BEM::StrDOF(idf)));
				}
			}
			if (tab.GetCount() >= idof + 6*ib && idof >= 0)
				tab.Set(idof + 6*ib);
		}
	
		idxHydro = -1;
		for (int row = 0; row < mbm.listLoaded.GetCount(); ++row) {
			if (mbm.listLoaded.IsSelected(row)) {
				idxHydro = ArrayModel_IndexHydro(mbm.listLoaded, row);
				break;
			}
		}	// Only one available => directly selected
		if (idxHydro < 0 && mbm.listLoaded.GetCount() == 1)
			idxHydro = ArrayModel_IndexHydro(mbm.listLoaded, 0);
		//if (idHydro < 0) 
		//	return false;
		
		//if (ArrayCtrlSelectedGetCount(mbm.listLoaded) > 1) 
		//	return false;
		
		// Show the tab if any model has QTFs
		bool show = false;
		for (const Hydro &hy : Bem().hydros) {
			if (hy.IsLoadedQTF(true) || hy.IsLoadedQTF(false)) {
				show = true;
				break;
			}
		}
		if (!show)
			return false;
		////
		
		if (idxHydro >= 0) {
			const Hydro &hy = Bem().hydros[idxHydro];
		
			opQTF.Clear();
			if (hy.IsLoadedQTF(true))
				opQTF.Add(FSUM, t_("Summation"));
			else 
				isSumm = false;
			if (hy.IsLoadedQTF(false))
				opQTF.Add(FDIFFERENCE, t_("Difference"));
			else
				isSumm = true;
			if (opQTF.GetCount() > 1)
				opQTF.SetIndex(isSumm ? FSUM : FDIFFERENCE);
			else if (opQTF.GetCount() > 0)
				opQTF.SetIndex(0);
				
			UArray<std::complex<double>> qh;					// Prepare qtf headings to be shown ordered
			for (const auto &c : hy.dt.qhead)
				qh << FixHeading(c, Bem().headingType);
			
			Sort(qh, SortComplex);
		
			ArrayCtrl &headQTF = mbm.menuPlot.headQTF;
			
			int row = headQTF.GetCursor();
			std::complex<double> val;
			if (row >= 0) {
				val.real(headQTF.Get(row, 0));
				val.imag(headQTF.Get(row, 1));
			}
			headQTF.Clear();
			for (int ih = 0; ih < qh.size(); ++ih) {
				if (val == qh[ih])
					row = ih;
				headQTF.Add(qh[ih].real(), qh[ih].imag());
			}
			if (row >= 0) 
				headQTF.SetCursor(row);
			else {
				for (int i = 0; i < dof.size(); ++i) {		// No QTF found. Clear surfs
					dof[i].up.surf.RemoveSurf();
					dof[i].down.surf.RemoveSurf();
				}
			}

			mbm.menuPlotList.SetQTF();
		}
	} catch (Exc e) {
		BEM::PrintError(DeQtfLf(e));
	}				
	return true;		
}

void MainQTF::Unload(int idf) {
	if (ib < 0)
		return;
	
	if (idf < 0) {
		idf = tab.Get();
		if (idf < 0)
			return;
	}
	
	posSplitter = dof[idf+6*ib].splitter.GetPos(0);
}
