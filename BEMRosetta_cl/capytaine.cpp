// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2020 - 2025, the BEMRosetta author and contributors
#include "BEMRosetta.h"
#include "BEMRosetta_int.h"
#include <NetCDF/NetCDF.h>


String CapyNC_Load(const char *file, UArray<Hydro> &hydros, int &num) {
	num = 0;
	
	try {
		BEM::Print(S("\n\n") + Format(t_("Loading '%s'"), file));
		BEM::Print(S("\n- ") + S(t_("NC file")));
		
		String name = GetFileTitle(file);
	
		NetCDFFile cdf(file);

		double _g = cdf.GetDouble("g");
		
		UVector<double> _rho;
		cdf.GetDouble("rho", _rho);

		UVector<double> _h;
		cdf.GetDouble("water_depth", _h);
		for (auto &hh : _h) {
			if (std::isinf(hh))
				hh = -1;
		}
		
		UVector<double> _head;
		cdf.GetDouble("wave_direction", _head);
		for (auto &hh : _head)
			hh = ToDeg(hh);
		int Nh = _head.size();
		
		UVector<double> _w;
		cdf.GetDouble("omega", _w);
		
		int Nftotal = _w.size();
		
		bool thereisw0 = _w[0] <= 0.0000001;
		if (thereisw0)
			_w.Remove(0);
		bool thereiswinf = std::isinf(_w[_w.size()-1]);
		if (thereiswinf)
			_w.Remove(_w.size()-1);
		
		int Nf = _w.size();

		int numaxisAB = 3;
		if (_rho.size() > 1)
			numaxisAB++;
		if (_h.size() > 1)
			numaxisAB++;
		
		MultiDimMatrixRowMajor<double> a;
		cdf.GetDouble("added_mass", a);
		if (!(numaxisAB == a.GetNumAxis()))
			throw Exc(t_("Wrong dimension in added_mass"));

		MultiDimMatrixRowMajor<double> b;
		cdf.GetDouble("radiation_damping", b);
		if (!(numaxisAB == b.GetNumAxis()))
			throw Exc(t_("Wrong dimension in radiation_damping"));
		
		int numaxisF = 4;
		if (_rho.size() > 1)
			numaxisF++;
		if (_h.size() > 1)
			numaxisF++;
		
		MultiDimMatrixRowMajor<double> sc;
		if (cdf.ExistVar("diffraction_force")) {
			cdf.GetDouble("diffraction_force", sc);
			if (!(numaxisF == sc.GetNumAxis()))
				throw Exc(t_("Wrong dimension in diffraction_force"));
		}
		
		MultiDimMatrixRowMajor<double> fk;
		if (cdf.ExistVar("Froude_Krylov_force")) {
			cdf.GetDouble("Froude_Krylov_force", fk);
			if (!(numaxisF == fk.GetNumAxis()))
				throw Exc(t_("Wrong dimension in Froude_Krylov_force"));
		}

		MultiDimMatrixRowMajor<double> rao;
		if (cdf.ExistVar("RAO")) {
			cdf.GetDouble("RAO", rao);
			if (!(numaxisF == rao.GetNumAxis()))
				throw Exc(t_("Wrong dimension in RAO"));
		}
		
		if (_rho.size() == 1) {		// Added to simplify handling later
			a.InsertAxis(0, 1);
			b.InsertAxis(0, 1);
			if (!sc.IsEmpty())
				sc.InsertAxis(0, 1);
			if (!fk.IsEmpty())
				fk.InsertAxis(0, 1);
			if (!rao.IsEmpty())
				rao.InsertAxis(0, 1);
		}
		if (_h.size() == 1) {
			a.InsertAxis(1, 1);
			b.InsertAxis(1, 1);
			if (!sc.IsEmpty())
				sc.InsertAxis(1, 1);
			if (!fk.IsEmpty())
				fk.InsertAxis(1, 1);
			if (!rao.IsEmpty())
				rao.InsertAxis(1, 1);
		}
		
		int Nb = a.size(3)/6;
		
		if (!(a.size(2) == Nftotal && a.size(3) == 6*Nb && a.size(4) == 6*Nb))
			throw Exc(t_("Wrong dimension in a"));
		if (!(b.size(2) == Nftotal && b.size(3) == 6*Nb && b.size(4) == 6*Nb))
			throw Exc(t_("Wrong dimension in b"));
				
		if (!sc.IsEmpty() && !(sc.size(2) == 2 && sc.size(3) == Nftotal && sc.size(4) == Nh && sc.size(5) == 6*Nb))
			throw Exc(t_("Wrong dimension in sc"));
		if (!fk.IsEmpty() && !(fk.size(2) == 2 && fk.size(3) == Nftotal && fk.size(4) == Nh && fk.size(5) == 6*Nb))
			throw Exc(t_("Wrong dimension in fk"));
		if (!rao.IsEmpty() && !(rao.size(2) == 2 && rao.size(3) == Nftotal && rao.size(4) == Nh && rao.size(5) == 6*Nb))
			throw Exc(t_("Wrong dimension in rao"));
		
		UArray<MatrixXd> M;
		if (cdf.ExistVar("inertia_matrix")) {
			MatrixXd _M;
			cdf.GetDouble("inertia_matrix", _M);
			M.SetCount(Nb);
			for (int ib = 0; ib < Nb; ++ib)
				M[ib] = _M.block(ib*6, ib*6, 6, 6);
		}
		
		UArray<MatrixXd> C;
		if (cdf.ExistVar("hydrostatic_stiffness")) {
			MatrixXd _C;
			cdf.GetDouble("hydrostatic_stiffness", _C);
			C.SetCount(Nb);
			for (int ib = 0; ib < Nb; ++ib)
				C[ib] = _C.block(ib*6, ib*6, 6, 6);
		}
		
		MatrixXd c0;
		if (cdf.ExistVar("rotation_center")) {
			cdf.GetDouble("rotation_center", c0);
			c0.transposeInPlace();
			if (!(c0.rows() == 3 && c0.cols() == Nb))
				throw Exc(t_("Wrong dimension in c0"));
		} else 
			c0 = MatrixXd::Zero(3, Nb);

		MatrixXd cg;
		if (cdf.ExistVar("center_of_mass")) {
			cdf.GetDouble("center_of_mass", cg);
			cg.transposeInPlace();
			if (!(cg.rows() == 3 && cg.cols() == Nb))
				throw Exc(t_("Wrong dimension in cg"));
		} else 
			cg = MatrixXd::Zero(3, Nb);
					
		String bodies = cdf.GetString("body_name");
		UVector<String> bds = Split(bodies, "+");	
		
		
		int numPan = 0;
		UVector<int> bodyPan;
		UVector<int> bodyIdEachPan;
		
		MultiDimMatrixRowMajor<double> dofDefinition;
		if (cdf.ExistVar("dof_definition")) {
			cdf.GetDouble("dof_definition", dofDefinition);
			if (!(3 == dofDefinition.GetNumAxis()))
				throw Exc(t_("Wrong dimension in dofDefinition"));
			if (!(dofDefinition.size(0) == 6*Nb && dofDefinition.size(2) == 3))
				throw Exc(t_("Wrong dimension in dofDefinition 2"));
			
			numPan = dofDefinition.size(1);
			bodyPan.SetCount(numPan);
			bodyIdEachPan.SetCount(numPan);
			UVector<int> bodyIdPan;
			bodyIdPan.SetCount(Nb, 0);
			
			auto GetBody = [=](int ip, const MultiDimMatrixRowMajor<double> &d)->int {
				for (int ib = 0; ib < Nb; ++ib) {
					for (int idof = 0; idof < 6; ++idof)
						for (int i = 0; i < 3; ++i)	
							if (d(idof + 6*ib, ip, i) != 0)
								return ib;
				}
				NEVER();	return -1;
			};
			for (int ip = 0; ip < numPan; ++ip) {
				int ib = bodyPan[ip] = GetBody(ip, dofDefinition);	// Assigns each panel to a body
				bodyIdEachPan[ip] = bodyIdPan[ib];					// Sets the id of each panel in its body
				bodyIdPan[ib]++;									// Increments the actual id of next panel in each body
			}
		}
				
		MultiDimMatrixRowMajor<double> pan;
		MultiDimMatrixRowMajor<double> rad_press, dif_press, inc_press;

		if (cdf.ExistVar("mesh_vertices")) {
			cdf.GetDouble("mesh_vertices", pan);
			if (!(3 == pan.GetNumAxis()))
				throw Exc(t_("Wrong dimension in mesh_vertices"));		
		
			numPan = pan.size(0);
			if (!(pan.size(1) == 4 && pan.size(2) == 3))
				throw Exc(t_("Wrong dimension in mesh_vertices 2"));
		}
		if (cdf.ExistVar("radiation_pressure")) {
			cdf.GetDouble("radiation_pressure", rad_press);
			if (!(numaxisF == rad_press.GetNumAxis()))
				throw Exc(t_("Wrong dimension in radiation_pressure"));		

			if (_rho.size() == 1)  		// Added to simplify handling later
				rad_press.InsertAxis(0, 1);
			if (_h.size() == 1) 
				rad_press.InsertAxis(1, 1);
		
			if (!(rad_press.size(2) == 2 && rad_press.size(3) == Nftotal && rad_press.size(4) == 6*Nb && rad_press.size(5) == numPan))
				throw Exc(t_("Wrong dimension in radiation_pressure 2"));
		}
		if (cdf.ExistVar("diffraction_pressure")) {
			cdf.GetDouble("diffraction_pressure", dif_press);
			if (!(numaxisF == dif_press.GetNumAxis()))
				throw Exc(t_("Wrong dimension in diffraction_pressure"));		
		
			if (_rho.size() == 1) 		// Added to simplify handling later
				dif_press.InsertAxis(0, 1);
			if (_h.size() == 1) 
				dif_press.InsertAxis(1, 1);
			
			if (!(dif_press.size(2) == 2 && dif_press.size(3) == Nftotal && dif_press.size(4) == Nh && dif_press.size(5) == numPan))
				throw Exc(t_("Wrong dimension in diffraction_pressure 2"));
		}
		if (cdf.ExistVar("incident_pressure")) {
			cdf.GetDouble("incident_pressure", inc_press);
			if (!(numaxisF == inc_press.GetNumAxis()))
				throw Exc(t_("Wrong dimension in incident_pressure"));		
		
			if (_rho.size() == 1) 		// Added to simplify handling later
				inc_press.InsertAxis(0, 1);				
			if (_h.size() == 1) 
				inc_press.InsertAxis(1, 1);
			if (!(inc_press.size(2) == 2 && inc_press.size(3) == Nftotal && inc_press.size(4) == Nh && inc_press.size(5) == numPan))
				throw Exc(t_("Wrong dimension in incident_pressure 2"));
		}

		auto LoadAB = [&](const MultiDimMatrixRowMajor<double> &_a, UArray<UArray<VectorXd>> &a, int irho, int ih) {
			int iwdelta = !thereisw0 ? 0 : 1;
			for (int r = 0; r < 6*Nb; ++r) 
				for (int c = 0; c < 6*Nb; ++c) 
					for (int iw = 0; iw < Nf; ++iw) 
						a[r][c](iw) = _a(irho, ih, iw + iwdelta, r, c);
		};
		auto LoadA0inf = [&](const MultiDimMatrixRowMajor<double> &_a, MatrixXd &a, bool is0, int irho, int ih) {
			int idw = is0 ? 0 : Nftotal-1;
			for (int r = 0; r < 6*Nb; ++r) 
				for (int c = 0; c < 6*Nb; ++c) 
					for (int iw = 0; iw < Nf; ++iw) 
						a(r, c) = _a(irho, ih, idw, r, c);
		};
		auto LoadForce = [&](const MultiDimMatrixRowMajor<double> &_f, Hydro::Forces &f, int irho, int _ih, int ib) {
			int iwdelta = !thereisw0 ? 0 : 1;
			for (int idf = 0; idf < 6; ++idf) 
				for (int ih = 0; ih < Nh; ++ih) 
					for (int iw = 0; iw < Nf; ++iw) 
						f[ib][ih](iw, idf) = std::complex<double>(_f(irho, _ih, 0, iw + iwdelta, ih, idf + 6*ib), 
																 -_f(irho, _ih, 1, iw + iwdelta, ih, idf + 6*ib));	//-Imaginary to follow Wamit
		};
		
		for (int irho = 0; irho < _rho.size(); ++irho) {
			for (int ih = 0; ih < _h.size(); ++ih) {
				Hydro &hy = hydros.Add();
				num++;
				
				hy.dt.file = file;
				hy.dt.name = name;
				if (_rho.size() > 1)
					hy.dt.name += Format("_rho%.0f", _rho[irho]);
				if (_h.size() > 1)
					hy.dt.name += Format("_h%.0f", _h[ih]);
				hy.dt.dimen = true;
				hy.dt.len = 1;
				hy.dt.solver = Hydro::CAPY_NC;
		
				hy.dt.x_w = hy.dt.y_w = 0;
				
				hy.dt.g = _g;
				hy.dt.rho = _rho[irho];
				hy.dt.h = _h[ih];
				
				hy.dt.Nb = Nb;
				
				hy.dt.w = clone(_w);

				hy.dt.Nf = Nf;
				hy.dt.head = clone(_head);
				hy.dt.Nh = Nh;

				hy.dt.msh.SetCount(Nb);
				for (int ib = 0; ib < Nb; ++ib) {
					for (int i = 0; i < 3; ++i)
						hy.dt.msh[ib].dt.c0[i] = c0(i, ib);
					for (int i = 0; i < 3; ++i)
						hy.dt.msh[ib].dt.cg[i] = cg(i, ib);
					if (bds.size() > ib)
						hy.dt.msh[ib].dt.name = bds[ib];
					if (M.size() > ib)
						hy.dt.msh[ib].dt.M = clone(M[ib]);
					if (C.size() > ib)
						hy.dt.msh[ib].dt.C = clone(C[ib]);
				}
				
				hy.Initialize_AB(hy.dt.A);
				hy.Initialize_AB(hy.dt.B);
				
				LoadAB(a, hy.dt.A, irho, ih);
				LoadAB(b, hy.dt.B, irho, ih);
				
				if (thereisw0) {
					hy.dt.A0.setConstant(6*hy.dt.Nb, 6*hy.dt.Nb, NaNDouble);
					LoadA0inf(a, hy.dt.A0, true, irho, ih);
				}
				if (thereiswinf) {
					hy.dt.Ainf.setConstant(6*hy.dt.Nb, 6*hy.dt.Nb, NaNDouble);
					LoadA0inf(a, hy.dt.Ainf, false, irho, ih);
				}
								
				if (!sc.IsEmpty())
					hy.Initialize_Forces(hy.dt.sc);
				if (!fk.IsEmpty())
					hy.Initialize_Forces(hy.dt.fk);
				if (!rao.IsEmpty())
					hy.Initialize_Forces(hy.dt.rao);
	
				for (int ib = 0; ib < Nb; ++ib) {
					if (!sc.IsEmpty())
						LoadForce(sc, hy.dt.sc, irho, ih, ib);
					if (!fk.IsEmpty())
						LoadForce(fk, hy.dt.fk, irho, ih, ib);
					if (!rao.IsEmpty())
						LoadForce(rao, hy.dt.rao, irho, ih, ib);
				}
				
				if (pan.size() > 0) {
					for (int ipall = 0; ipall < numPan; ++ipall) {
						int ib = bodyPan[ipall];
						Surface &msh = hy.dt.msh[ib].dt.mesh;
						Panel &p = msh.panels.Add();
				
						for (int i = 0; i < 4; ++i) {
							Point3D pnt(pan(ipall, i, 0), pan(ipall, i, 1), pan(ipall, i, 2));
							p.id[i] = FindAdd(msh.nodes, pnt);
						}
					}
				}
				
				int iwdelta = !thereisw0 ? 0 : 1;
				
				if (rad_press.size() > 0) {
					hy.Initialize_PotsRad();
					
					for (int ipall = 0; ipall < numPan; ++ipall) {
						int ib = bodyPan[ipall];
						int ip = bodyIdEachPan[ipall];
						for (int ifr = 0; ifr < Nf; ++ifr) {
							double rw = hy.dt.rho*sqr(hy.dt.w[ifr]);							
							for (int idf = 0; idf < 6; ++idf) {
								double re = rad_press(irho, ih, 0, ifr + iwdelta, idf + 6*ib, ipall);
								double im = rad_press(irho, ih, 1, ifr + iwdelta, idf + 6*ib, ipall);
								hy.dt.pots_rad[ib][ip][idf][ifr] += std::complex<double>(-re, im)/rw; // p = -iρωΦ ; Φ = [Im(p) - iRe(p)]/ρω
							}
						}
					}
				}
				if (inc_press.size() > 0) {
					hy.Initialize_PotsIncDiff(hy.dt.pots_inc);
					
					for (int ipall = 0; ipall < numPan; ++ipall) {
						int ib = bodyPan[ipall];
						int ip = bodyIdEachPan[ipall];
						for (int ifr = 0; ifr < Nf; ++ifr) {
							double rw = hy.dt.rho*hy.dt.w[ifr];
							for (int ihead = 0; ihead < Nh; ++ihead) {
								double re = inc_press(irho, ih, 0, ifr + iwdelta, ihead, ipall);
								double im = inc_press(irho, ih, 1, ifr + iwdelta, ihead, ipall);
								hy.dt.pots_inc[ib][ip][ihead][ifr] += std::complex<double>(im, re)/rw; // p = -iρωΦ ; Φ = [Im(p) - iRe(p)]/ρω
							}
						}
					}
				}
				if (dif_press.size() > 0) {
					hy.Initialize_PotsIncDiff(hy.dt.pots_dif);
					
					for (int ipall = 0; ipall < numPan; ++ipall) {
						int ib = bodyPan[ipall];
						int ip = bodyIdEachPan[ipall];
						for (int ifr = 0; ifr < Nf; ++ifr) {
							double rw = hy.dt.rho*hy.dt.w[ifr];
							for (int ihead = 0; ihead < Nh; ++ihead) {
								double re = dif_press(irho, ih, 0, ifr + iwdelta, ihead, ipall);
								double im = dif_press(irho, ih, 1, ifr + iwdelta, ihead, ipall);
								hy.dt.pots_dif[ib][ip][ihead][ifr] += std::complex<double>(im, re)/rw; // p = -iρωΦ ; Φ = [Im(p) - iRe(p)]/ρω
							}
						}
					}
				}
			}
		}
	} catch (Exc e) {
		return e;
	}
	return String();
}

void Nemoh::SaveCase_Capy(String folder, int numThreads, bool withPotentials, bool withMesh, bool x0z, bool y0z, const UArray<Body> &lids) const {
	DirectoryCreate(folder);
	String name = GetFileTitle(folder);
	String fileBat  = AFX(folder, "Capytaine_bat.bat");
	FileOut bat;
	if (!bat.Open(fileBat))
		throw Exc(Format(t_("Impossible to open file '%s'"), fileBat));
	
	bat << "echo Start: \%date\% \%time\% > time.txt\n";
	if (!IsNull(numThreads) && numThreads > 0) 
		bat << "set OMP_NUM_THREADS=" << numThreads << "\n"
			<< "set MKL_NUM_THREADS=" << numThreads << "\n";
	if (!Bem().pythonEnv.IsEmpty()) {
		if (Bem().pythonEnv.Find(' ') > 0)
			bat << Bem().pythonEnv << "\n";
		else
			bat << "call conda activate " << Bem().pythonEnv << "\n";
	}
	bat << "python \"" << name << ".py\"\n";
	//bat << "@IF \%ERRORLEVEL\% NEQ 0 PAUSE \"Error\"";
	bat << "\necho End:   \%date\% \%time\% >> time.txt";
	
	String filePy  = AFX(folder, name + ".py");
	String spy;
	
	spy <<	"# Code generated by BEMRosetta for Capytaine from version 2.3\n"
			"import numpy as np\n"
			"import capytaine as cpt\n"
			"from capytaine.io.xarray import problems_from_dataset\n"
			"from capytaine.bem.airy_waves import airy_waves_pressure\n"
			"from capytaine.post_pro.rao import rao\n"
			"import xarray as xr\n"
			"import os\n\n"
			"print(f'Capytaine version is: {cpt.__version__}')\n\n";

	String listBodies;
	
	String folderMesh = AFX(folder, "mesh");
	if (!DirectoryCreateX(folderMesh))
		throw Exc(Format(t_("Problem creating '%s' folder"), folderMesh));

	bool automaticLid = false;
	bool dorao = false;
	
	for (int ib = 0; ib < dt.Nb; ++ib) {
		const Body &b = dt.msh[ib];
		
		String dest = AFX(folderMesh, Format(t_("Body_%d.dat"), ib+1));
		Body::SaveAs(b, dest, Body::NEMOH_DAT, Body::UNDERWATER, dt.rho, dt.g, false, dt.symY);
		
		spy <<	Format("mesh_%d = cpt.load_mesh('./mesh/%s', file_format='nemoh')\n", ib+1, GetFileName(dest));
		
		bool isLid = lids.size() > ib && !lids[ib].dt.mesh.panels.IsEmpty();
		if (isLid) {
			String destLid = AFX(folderMesh, Format(t_("Body_%d_lid.dat"), ib+1));
			Body::SaveAs(lids[ib], destLid, Body::NEMOH_DAT, Body::ALL, dt.rho, dt.g, false, dt.symY);
			spy << Format("lid_mesh_%d = cpt.load_mesh('./mesh/%s', file_format='nemoh')\n", ib+1, GetFileName(destLid));
		} else if (automaticLid)
			spy << Format("lid_mesh_%d = mesh_%d.translated_z(1e-7).generate_lid()     # See https://github.com/capytaine/capytaine/issues/589\n", ib+1, ib+1);
		
		spy <<	"\n";
		
		spy << 	Format("body_%d = cpt.FloatingBody(mesh=mesh_%d,%s dofs=cpt.rigid_body_dofs(rotation_center=(%f, %f, %f)), center_of_mass=(%f, %f, %f), name='%s')\n\n", 
					ib+1, ib+1, 
					(isLid || automaticLid ? Format("lid_mesh=lid_mesh_%d, ", ib+1) : S("")),
					b.dt.c0.x, b.dt.c0.y, b.dt.c0.z,
					b.dt.cg.x, b.dt.cg.y, b.dt.cg.z,
					b.dt.name
					);
		
		if (b.dt.M.size() == 36 && b.dt.M.cwiseAbs().maxCoeff() != 0) {
			dorao = true;
			spy <<  Format("body_%d.inertia_matrix = [\n", ib+1);
			for (int r = 0; r < 6; ++r) {
				spy << "    [";
				for (int c = 0; c < 6; ++c) {
					if (c > 0)
						spy << ", ";
					spy << Format("%f", b.dt.M(r, c));
				}
				spy << "]";
				if (r < 6-1)
					spy << ",";
				spy << "\n";
			}
			spy << "]\n\n";
		}
		spy <<	Format("body_%d.hydrostatic_stiffness = body_%d.compute_hydrostatic_stiffness()\n", ib+1, ib+1);

		spy <<	"\n";
		
		if (!listBodies.IsEmpty())
			listBodies << ", ";
		listBodies << Format("body_%d", ib+1);
	}
	spy << "list_of_bodies = [" << listBodies << "]\n";

	String somega = "[", shead = "[";
	for (int iw = 0; iw < dt.w.size(); ++iw) {
		if (iw > 0)
			somega << ", ";
		somega << dt.w[iw];
	}
	somega << "]";
	for (int ih = 0; ih < dt.head.size(); ++ih) {
		if (ih > 0)
			shead << ", ";
		shead << ToRad(dt.head[ih]);
	}
	shead << "]";
	
	spy <<	"all_bodies = cpt.FloatingBody.join_bodies(*list_of_bodies)\n"
			"test_matrix = xr.Dataset(coords={\n"
		    "    'omega': " << somega << ",\n"
		    "    'wave_direction': " << shead << ",\n"
		    "    'radiating_dof': list(all_bodies.dofs),\n"
		    "    'water_depth': " << (dt.h > 0 ? FormatDouble(dt.h) : "np.inf") << ",\n"
		    "    'rho': " << dt.rho << "\n"
		    "})\n\n";

	spy <<	"solver = cpt.BEMSolver()\n"
			"pbs = problems_from_dataset(test_matrix, all_bodies)\n"
			"results = solver.solve_all(pbs, keep_details=True)\n"
			"ds = cpt.assemble_dataset(results)\n"
			"\n"
			"mesh = all_bodies.mesh\n"
			"\n";
	
	MatrixXd Dlin(6*dt.Nb,6 *dt.Nb);
	Dlin.setZero();
	
	for (int ib = 0; ib < dt.Nb; ++ib) {
		if (dt.msh[0].dt.Dlin.size() == 36)
			Dlin.block(6*ib, 6*ib, 6, 6) = dt.msh[0].dt.Dlin;
	}
	
	if (dorao) {
		if (Dlin.size() == 36 && Dlin.cwiseAbs().maxCoeff() != 0) {
			spy <<  "my_dissipation = all_bodies.add_dofs_labels_to_matrix([\n";
			for (int r = 0; r < 6*dt.Nb; ++r) {
				spy << "    [";
				for (int c = 0; c < 6*dt.Nb; ++c) {
					if (c > 0)
						spy << ", ";
					spy << Dlin(r, c);
				}
				spy << "]";
				if (r < 6*dt.Nb-1)
					spy << ",";
				spy << "\n";
			}
			spy << "])\n\n";
		}
		spy <<	"ds['RAO'] = rao(ds";
		
		if (Dlin.cwiseAbs().maxCoeff() != 0) 
			spy << ", dissipation = my_dissipation";
		
		spy <<	")\n";
	}
	
	spy <<	"ds.coords['space_coordinate'] = ['x', 'y', 'z']\n";
	if (withMesh) 
		spy <<	"ds['mesh_vertices'] = (['face', 'vertices_of_face', 'space_coordinate'], mesh.vertices[mesh.faces])\n"
				"ds['mesh_faces_center'] = (['face', 'space_coordinate'], mesh.faces_centers)\n";
				
	spy <<	"ds['dof_definition'] = (['radiating_dof', 'face', 'space_coordinate'], np.array([all_bodies.dofs[dof] for dof in all_bodies.dofs]))\n"
			"\n";
	
	if (withMesh && withPotentials) {
		spy <<	"ds['incident_pressure'] = (\n"
				"    ['omega', 'wave_direction', 'face'],\n"
				"    np.zeros((ds.sizes['omega'], ds.sizes['wave_direction'], all_bodies.mesh.nb_faces,), dtype=np.complex128),\n"
				")\n"
				"ds['diffraction_pressure'] = (\n"
				"    ['omega', 'wave_direction', 'face'],\n"
				"    np.zeros((ds.sizes['omega'], ds.sizes['wave_direction'], all_bodies.mesh.nb_faces), dtype=np.complex128),\n"
				")\n"
				"ds['radiation_pressure'] = (\n"
				"    ['omega', 'radiating_dof', 'face'],\n"
				"    np.zeros((ds.sizes['omega'], ds.sizes['radiating_dof'], all_bodies.mesh.nb_faces), dtype=np.complex128),\n"
				")\n"
				"\n";
	
		spy <<	"for res in results:\n"
    			"    if isinstance(res.problem, cpt.DiffractionProblem):\n"
        		"        ds['diffraction_pressure'].loc[dict(omega=res.omega, wave_direction=res.wave_direction)] = res.pressure[:mesh.nb_faces]\n"
        		"        ds['incident_pressure'].loc[dict(omega=res.omega, wave_direction=res.wave_direction)] = airy_waves_pressure(mesh, res)\n"
    			"    elif isinstance(res.problem, cpt.RadiationProblem):\n"
        		"        ds['radiation_pressure'].loc[dict(omega=res.omega, radiating_dof=res.radiating_dof)] = res.pressure[:mesh.nb_faces]\n"
        		"\n";

	}
	spy <<	"ds.coords['rigid_body_component'] = [body.name for body in list_of_bodies]\n"
			"ds['rotation_center'] = (['rigid_body_component', 'point_coordinates'], [body.rotation_center for body in list_of_bodies])\n"
			"ds['center_of_mass'] = (['rigid_body_component', 'point_coordinates'], [body.center_of_mass for body in list_of_bodies])\n"
			"\n"
			"# Export to NetCDF file\n"
			"cpt.export_dataset('" << name << ".nc', ds, format=\"netcdf\")\n";
			//"from capytaine.io.xarray import separate_complex_values\n"
			//"separate_complex_values(ds).to_netcdf('" << name << ".nc',\n"
			//"                                          encoding={'radiating_dof': {'dtype': 'U'},\n"
			//"                                                    'influenced_dof': {'dtype': 'U'}})\n";	
	
	spy.Replace("'", "\"");
	spy.Replace("\\", "\\\\");
	
	FileOut py;
	if (!py.Open(filePy))
		throw Exc(Format(t_("Impossible to open file '%s'"), fileBat));
	
	py << spy;
};


//	void Initialize_PotsRad();
//	void Initialize_PotsIncDiff(UArray<UArray<UArray<UArray<std::complex<double>>>>> &pots);
	
// diffraction_force		rexim x Nf x Nh x 6xNb


// omega					Nf
// period
// wave_direction			Nh
// inertia_matrix			6xNb x 6xNb
// hydrostatic_stiffness
// added_mass				2 x 2 x Nf x 6xNb x 6xNb
// radiation_damping
// diffraction_force		2 x 2 x re/im x Nf x Nh x 6xNb
// Froude_Krylov_force
// excitation_force 

// A						[6*Nb][6*Nb][Nf]
// C						[Nb](6, 6)
// M
// Force					[Nh](Nf, 6*Nb) 
  


