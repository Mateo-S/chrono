//////////////////////////////////////////////////
//  
//   ChPovRay.cpp
//
// ------------------------------------------------
// 	 Copyright:Alessandro Tasora / DeltaKnowledge
//             www.deltaknowledge.com
// ------------------------------------------------
///////////////////////////////////////////////////

    
  
#include "ChPovRay.h"
#include "geometry/ChCTriangleMeshConnected.h"
#include "assets/ChObjShapeFile.h"
#include "assets/ChSphereShape.h"
#include "assets/ChBoxShape.h"
#include "assets/ChTexture.h"
#include "assets/ChAssetLevel.h"
#include "assets/ChCamera.h"
#include "ChPovRayAssetCustom.h"
#include "physics/ChParticlesClones.h"
#include "physics/ChLinkMate.h"
#include "physics/ChContactContainer.h"

using namespace chrono;
using namespace postprocess;
using namespace geometry;

 
ChPovRay::ChPovRay(ChSystem* system) : ChPostProcessBase(system)
{
	this->pic_filename		= "pic";
	this->template_filename = "../data/_template_POV.pov";
	this->out_script_filename = "render_frames.pov";
	this->out_data_filename = "state";
	this->framenumber = 0;
	this->camera_location = ChVector<>(0,1.5,-2);
	this->camera_aim = ChVector<>(0,0,0);
	this->camera_up = ChVector<>(0,1,0);
	this->camera_angle =30;
	this->camera_orthographic = false;
	this->camera_found_in_assets = false;
	this->def_light_location = ChVector<>(2,3,-1);
	this->def_light_color = ChColor(1,1,1);
	this->def_light_cast_shadows = true;
	this->background = ChColor(1,1,1);
	this->antialias = false;
	this->antialias_depth = 2;
	this->antialias_treshold = 0.1;
	this->picture_height = 600;
	this->picture_width = 800;
	this->ambient_light = ChColor(2,2,2);
	this->COGs_show = false;
	this->COGs_size = 0.04;
	this->frames_show = false;
	this->frames_size = 0.05;
	this->links_show = false;
	this->links_size = 0.04;
	this->contacts_show = false;
	this->contacts_maxsize = 0.1;
	this->contacts_scale = 0.01;
	this->contacts_scale_mode =  SYMBOL_VECTOR_SCALELENGTH;
	this->contacts_width  = 0.001;
	this->contacts_colormap_startscale = 0;
	this->contacts_colormap_endscale = 10;
	this->contacts_do_colormap = true;
}

void ChPovRay::AddAll()
{
	ChSystem::IteratorBodies myiter = mSystem->IterBeginBodies();
	while (myiter != mSystem->IterEndBodies())
	{	
		this->mdata.push_back((*myiter));
		++myiter;
	}
	ChSystem::IteratorOtherPhysicsItems myiterp = mSystem->IterBeginOtherPhysicsItems();
	while (myiterp != mSystem->IterEndOtherPhysicsItems())
	{	
		this->mdata.push_back((*myiterp));
		++myiterp;
	}
	ChSystem::IteratorLinks myiterl = mSystem->IterBeginLinks();
	while (myiterl != mSystem->IterEndLinks())
	{	
		this->mdata.push_back((*myiterl));
		++myiterl;
	}

}

void ChPovRay::Add(ChSharedPtr<ChPhysicsItem> mitem)
{
	this->mdata.push_back(mitem);
}

void ChPovRay::RemoveAll()
{
	this->mdata.clear();
}



std::string replaceOnce(
  std::string result, 
  const std::string& replaceWhat, 
  const std::string& replaceWithWhat)
{
  const int pos = result.find(replaceWhat);
  if (pos==-1) return result;
  result.replace(pos,replaceWhat.size(),replaceWithWhat);
  return result;
}


std::string replaceAll(
  std::string result, 
  const std::string& replaceWhat, 
  const std::string& replaceWithWhat)
{
  while(1)
  {
    const int pos = result.find(replaceWhat);
    if (pos==-1) break;
    result.replace(pos,replaceWhat.size(),replaceWithWhat);
  }
  return result;
}


void ChPovRay::SetCamera(ChVector<> location, ChVector<> aim, double angle, bool ortho)
{
	this->camera_location = location;
	this->camera_aim = aim;
	this->camera_angle = angle;
	this->camera_orthographic = ortho;
}

void ChPovRay::SetLight(ChVector<> location, ChColor color, bool cast_shadow)
{
	this->def_light_location = location;
	this->def_light_color = color;
	this->def_light_cast_shadows = cast_shadow;
}

void ChPovRay::SetShowCOGs(bool show, double msize)
{
	this->COGs_show = show;
	if (show) 
		this->COGs_size = msize;
}
void ChPovRay::SetShowFrames(bool show, double msize)
{
	this->frames_show = show;
	if (show) 
		this->frames_size = msize;
}
void ChPovRay::SetShowLinks(bool show, double msize)
{
	this->links_show = show;
	if (show) 
		this->links_size = msize;
}
void ChPovRay::SetShowContacts(bool show, eChContactSymbol mode, double scale, double width, double max_size, bool do_colormap, double colormap_start, double colormap_end)
{
	this->contacts_show = show;
	if (show)
	{
		this->contacts_scale_mode = mode;
		this->contacts_scale = scale;
		this->contacts_width = width;
		this->contacts_maxsize = max_size;
		this->contacts_do_colormap = do_colormap;
		this->contacts_colormap_startscale = colormap_start;
		this->contacts_colormap_endscale = colormap_end;
	}
}


void ChPovRay::ExportScript(const std::string &filename)
{
	this->out_script_filename = filename;
	
	pov_assets.clear();

	// Generate the _assets.pov script (initial state, it will be populated later by
	// appending assets as they enter the exporter, only once if shared, using ExportAssets() )

	std::string assets_filename = filename + ".assets";
	{
		ChStreamOutAsciiFile assets_file(assets_filename.c_str());
		assets_file << "// File containing meshes and objects for rendering POV scenes.\n";
		assets_file << "// This file is automatically included by " << filename.c_str() << ".pov , \n";
		assets_file << "// and you should not modify it.\n\n";
	}

	// Generate the .INI script
	std::string ini_filename = filename + ".ini";

	ChStreamOutAsciiFile ini_file(ini_filename.c_str());

	ini_file << "; Script for rendering an animation with POV-Ray. \n";
	ini_file << "; Generated autumatically by Chrono::Engine. \n\n";
	if (this->antialias)
		ini_file << "Antialias=On \n";
	else
		ini_file << "Antialias=Off \n";
	ini_file << "Antialias_Threshold=" << this->antialias_treshold << " \n";
	ini_file << "Antialias_Depth=" << this->antialias_depth << " \n";
	ini_file << "Height=" << this->picture_height << " \n";
	ini_file << "Width =" << this->picture_width << " \n";
	ini_file << "Input_File_Name=" << out_script_filename << "\n";
	ini_file << "Output_File_Name=" << pic_filename << "\n";
	ini_file << "Initial_Frame=0000 \n";
	ini_file << "Final_Frame=0999 \n";
	ini_file << "Initial_Clock=0 \n";
	ini_file << "Final_Clock=1 \n";
	ini_file << "Pause_when_Done=off \n";


	// Generate the .POV script:

	ChStreamOutAsciiFile mfile(filename.c_str());

	// Rough way to load the template head file in the string buffer
	if (template_filename != "")
	{
		ChStreamInAsciiFile templatefile(template_filename.c_str());
		std::string buffer_template;
		while(!templatefile.End_of_stream())
		{
			char m;
			try{
				templatefile >> m;
			} catch(ChException mex){};

			buffer_template += m;
		}

		// Do template replacement of [xxxyyyzzz] keywords, if any
		replaceAll(buffer_template, "[xxxyyyzzz]", "blabla" );
		
		mfile << buffer_template;
	}
	
	// Write default global settings and background

	mfile <<	"global_settings { \n" << 
				" ambient_light rgb<" << ambient_light.R <<","<< ambient_light.G <<","<< ambient_light.B <<"> \n"; 
	mfile <<	"}\n\n\n";
	mfile <<	"background { \n" << 
				" rgb <" << background.R <<","<< background.G <<","<< background.B <<"> \n"; 
	mfile <<	"}\n\n\n";

	// Write default camera
   
	mfile <<	"camera { \n";   
			if (camera_orthographic) 
			{
					mfile << " orthographic \n";
					mfile <<	" right x * " << (camera_location - camera_aim).Length() << " * tan ((( " << camera_angle << " *0.5)/180)*3.14) \n";
					mfile <<	" up y * image_height/image_width * " << (camera_location - camera_aim).Length() << " * tan (((" << camera_angle << "*0.5)/180)*3.14) \n";
					ChVector<> mdir = (camera_aim - camera_location)*0.00001;
					mfile <<	" direction <" << mdir.x << "," << mdir.y << "," << mdir.z << "> \n";
			}
			else
			{
					mfile <<	" right -x*image_width/image_height \n";
					mfile <<	" angle " << camera_angle << " \n";
			}
					mfile << " location <" << camera_location.x <<","<<camera_location.y <<","<< camera_location.z <<"> \n" <<
						" look_at <" << camera_aim.x <<","<<camera_aim.y <<","<<camera_aim.z <<"> \n" << 
						" sky <" << camera_up.x <<","<<camera_up.y <<","<<camera_up.z <<"> \n";
			mfile <<	"}\n\n\n"; 

	// Write default light

	mfile <<	"light_source { \n" << 
				" <" << def_light_location.x <<","<< def_light_location.y <<","<< def_light_location.z <<"> \n" <<
				" color rgb<" << def_light_color.R <<","<< def_light_color.G <<","<<def_light_color.B <<"> \n"; 
	if (!def_light_cast_shadows) 
		mfile <<" shadowless \n";
	mfile <<	"}\n\n\n"; 

	// Write POV custom code

	if ( this->custom_script.size() >0)
	{
		mfile << "// Custom user-added script: \n\n";
		mfile << this->custom_script;
		mfile << "\n\n";
	}

	// Write POV code to open the asset file

	mfile << "// Include shape assets (triangle meshes): \n\n";
	mfile << "#include \"" << assets_filename << "\"\n\n";

	// Write POV code to open the n.th scene file

	mfile << "// Include POV code to for the n.th scene file: \n\n";
	mfile << "#declare scene_file = concat(\"" << this->out_data_filename << "\", str(frame_number,-5,0), \".pov\") \n"; 
	mfile << "#include scene_file \n\n";

	// Write POV code to load and display contacts

	if (this->contacts_show)
	{
		mfile << "// Load contacts and create objects to show them: \n\n";

		mfile << "#declare contacts_scale=" << this->contacts_scale <<";\n" ;
		mfile << "#declare contacts_width=" << this->contacts_width <<";\n" ; 
		mfile << "#declare contacts_maxsize=" << this->contacts_maxsize <<";\n" ;
		mfile << "#declare contacts_do_colormap=" << ((int)this->contacts_do_colormap) <<";\n" ;
		mfile << "#declare contacts_colormap_startscale=" << this->contacts_colormap_startscale <<";\n" ; 
		mfile << "#declare contacts_colormap_endscale=" << this->contacts_colormap_endscale <<";\n" ;
		mfile << "#declare contacts_defaultcolor= rgb<0.0,0.9,0.2>; \n" ;
		if (this->contacts_scale_mode== SYMBOL_VECTOR_SCALELENGTH)
		{
			mfile << "#declare contacts_scale_mode=1;\n" ;
			mfile << "#declare draw_contacts_asspheres =0;\n" ;
			mfile << "#declare draw_contacts_ascylinders =1;\n" ;
		}
		if (this->contacts_scale_mode== SYMBOL_VECTOR_SCALERADIUS)
		{
			mfile << "#declare contacts_scale_mode=2;\n" ;
			mfile << "#declare draw_contacts_asspheres =0;\n" ;
			mfile << "#declare draw_contacts_ascylinders =1;\n" ;
		}
		if (this->contacts_scale_mode== SYMBOL_VECTOR_NOSCALE)
		{
			mfile << "#declare contacts_scale_mode=0;\n" ;
			mfile << "#declare draw_contacts_asspheres =0;\n" ;
			mfile << "#declare draw_contacts_ascylinders =1;\n" ;
		}
		if (this->contacts_scale_mode== SYMBOL_SPHERE_SCALERADIUS)
		{
			mfile << "#declare contacts_scale_mode=1;\n" ;
			mfile << "#declare draw_contacts_asspheres =1;\n" ;
			mfile << "#declare draw_contacts_ascylinders =0;\n" ;
		}
		if (this->contacts_scale_mode== SYMBOL_VECTOR_NOSCALE)
		{
			mfile << "#declare contacts_scale_mode=0;\n" ;
			mfile << "#declare draw_contacts_asspheres =1;\n" ;
			mfile << "#declare draw_contacts_ascylinders =0;\n" ;
		}
		mfile << "\n" ;

		mfile << "#declare contacts_file = concat(\"" << this->out_data_filename << "\", str(frame_number,-5,0), \".contacts\") \n";
		mfile << "#fopen MyContactsFile contacts_file read \n";

		mfile << " \n\
			union{\n\
				#read (MyContactsFile, apx, apy, apz, anx, any, anz,  afx, afy, afz ) \n\
				#while (defined(MyContactsFile)) \n\
					#read (MyContactsFile, apx, apy, apz, anx, any, anz,  afx, afy, afz ) \n\
					make_contact(apx, apy, apz, anx, any, anz,  afx, afy, afz) \n\
				#end  \n\
			} \n";
	}

	// Populate the assets
	this->ExportAssets();

}

void ChPovRay::_recurseExportAssets(std::vector< ChSharedPtr<ChAsset> >& assetlist, ChStreamOutAsciiFile& assets_file)
{
	// Scan assets 
	for (unsigned int k = 0; k < assetlist.size(); k++)
	{
		ChSharedPtr<ChAsset> k_asset = assetlist[k];

		ChHashTable<unsigned int, ChSharedPtr<ChAsset> >::iterator mcached = pov_assets.find( (unsigned int)k_asset.get_ptr() );
		if (mcached == pov_assets.end())
		{
			// Ok, add the asset in POV file, it was not already saved. 
			// Otherwise it was a shared asset.
			pov_assets.insert((unsigned int)k_asset.get_ptr(), k_asset);

			// Do dynamic casting of the shared pointer to see which type
			// of asset is contained...


			// *) asset k of object i references an .obj wavefront mesh?
			if (k_asset.IsType<ChObjShapeFile>() )
			{
				ChSharedPtr<ChObjShapeFile> myobjshapeasset(k_asset);
				ChTriangleMeshConnected mytrimesh;

				try {
					// Load from the .obj file and convert.
					mytrimesh.LoadWavefrontMesh( myobjshapeasset->GetFilename(), true, true );

					// POV macro to build the asset - begin
					assets_file << "#macro sh_"<< (int) k_asset.get_ptr() << "()\n"; //"(apx, apy, apz, aq0, aq1, aq2, aq3)\n";

					// Create mesh
					assets_file << "mesh2  {\n";

					assets_file << " vertex_vectors {\n";
					assets_file << (int)mytrimesh.m_vertices.size() << ",\n";
					for (unsigned int iv = 0; iv < mytrimesh.m_vertices.size(); iv++)
						assets_file << "  <" << mytrimesh.m_vertices[iv].x << "," <<  mytrimesh.m_vertices[iv].y << "," <<  mytrimesh.m_vertices[iv].z << ">,\n";
					assets_file <<" }\n";

					assets_file << " normal_vectors {\n";
					assets_file << (int)mytrimesh.m_normals.size() << ",\n";
					for (unsigned int iv = 0; iv < mytrimesh.m_normals.size(); iv++)
						assets_file << "  <" << mytrimesh.m_normals[iv].x << "," <<  mytrimesh.m_normals[iv].y << "," <<  mytrimesh.m_normals[iv].z << ">,\n";
					assets_file <<" }\n";

					assets_file << " uv_vectors {\n";
					assets_file << (int)mytrimesh.m_UV.size() << ",\n";
					for (unsigned int iv = 0; iv < mytrimesh.m_UV.size(); iv++)
						assets_file << "  <" << mytrimesh.m_UV[iv].x << "," <<  mytrimesh.m_UV[iv].y << ">,\n";
					assets_file <<" }\n";

					assets_file << " face_indices {\n";
					assets_file << (int)mytrimesh.m_face_v_indices.size() << ",\n";
					for (unsigned int it = 0; it < mytrimesh.m_face_v_indices.size(); it++)
						assets_file << "  <" << mytrimesh.m_face_v_indices[it].x << "," <<  mytrimesh.m_face_v_indices[it].y << "," <<  mytrimesh.m_face_v_indices[it].z << ">,\n";
					assets_file <<" }\n";

					//if (mytrimesh.m_normals.size() != mytrimesh.m_vertices.size())
					if (mytrimesh.m_face_n_indices != mytrimesh.m_face_v_indices)
					{
						assets_file << " normal_indices {\n";
						assets_file << (int)mytrimesh.m_face_n_indices.size() << ",\n";
						for (unsigned int it = 0; it < mytrimesh.m_face_n_indices.size(); it++)
							assets_file << "  <" << mytrimesh.m_face_n_indices[it].x << "," <<  mytrimesh.m_face_n_indices[it].y << "," <<  mytrimesh.m_face_n_indices[it].z << ">,\n";
						assets_file <<" }\n";
					}
					if (mytrimesh.m_face_u_indices != mytrimesh.m_face_v_indices)
					{
						assets_file << " uv_indices {\n";
						assets_file << (int)mytrimesh.m_face_u_indices.size() << ",\n";
						for (unsigned int it = 0; it < mytrimesh.m_face_u_indices.size(); it++)
							assets_file << "  <" << mytrimesh.m_face_u_indices[it].x << "," <<  mytrimesh.m_face_u_indices[it].y << "," <<  mytrimesh.m_face_u_indices[it].z << ">,\n";
						assets_file <<" }\n";
					}

					/*
					assets_file <<" pigment {color rgbt <" << 
						myobjshapeasset->GetColor().R << "," << 
						myobjshapeasset->GetColor().G << "," << 
						myobjshapeasset->GetColor().B << "," << 
						myobjshapeasset->GetFading() << "> }\n";
					*/

					assets_file <<"}\n";

					// POV macro - end
					assets_file << "#end \n";
				} 
				catch (ChException)
				{
					char error[400];
					sprintf(error,"Asset n.%d: can't read .obj file %s", k,myobjshapeasset->GetFilename().c_str() );
					throw (ChException(error));
				}
			}


			// *) asset k of object i is a sphere ?
			if (k_asset.IsType<ChSphereShape>() )
			{
				ChSharedPtr<ChSphereShape> myobjshapeasset(k_asset);

				// POV macro to build the asset - begin
				assets_file << "#macro sh_"<< (int) k_asset.get_ptr() << "()\n"; //"(apx, apy, apz, aq0, aq1, aq2, aq3)\n";

				// POV will make the sphere
				assets_file << "sphere  {\n";

				assets_file << " <" << myobjshapeasset->GetSphereGeometry().center.x;
				assets_file << ","  << myobjshapeasset->GetSphereGeometry().center.y;
				assets_file << ","  << myobjshapeasset->GetSphereGeometry().center.z << ">\n";
				assets_file << " "  << myobjshapeasset->GetSphereGeometry().rad << "\n";

				/*
				assets_file <<" pigment {color rgbt <" << 
						myobjshapeasset->GetColor().R << "," << 
						myobjshapeasset->GetColor().G << "," << 
						myobjshapeasset->GetColor().B << "," << 
						myobjshapeasset->GetFading() << "> }\n";
				*/

				assets_file <<"}\n";

				// POV macro - end 
				assets_file << "#end \n";
			}


			// *) asset k of object i is a box ?
			if (k_asset.IsType<ChBoxShape>() )
			{
				ChSharedPtr<ChBoxShape> myobjshapeasset(k_asset);

				// POV macro to build the asset - begin
				assets_file << "#macro sh_"<< (int) k_asset.get_ptr() << "()\n"; //"(apx, apy, apz, aq0, aq1, aq2, aq3)\n";

				// POV will make the box
				assets_file << "union  {\n";
				assets_file << "box  {\n";

				assets_file << " <" << -myobjshapeasset->GetBoxGeometry().Size.x;
				assets_file << ","  << -myobjshapeasset->GetBoxGeometry().Size.y;
				assets_file << ","  << -myobjshapeasset->GetBoxGeometry().Size.z << ">\n";
				assets_file << " <" <<  myobjshapeasset->GetBoxGeometry().Size.x;
				assets_file << ","  <<  myobjshapeasset->GetBoxGeometry().Size.y;
				assets_file << ","  <<  myobjshapeasset->GetBoxGeometry().Size.z << ">\n";

				ChQuaternion<> boxrot = myobjshapeasset->GetBoxGeometry().Rot.Get_A_quaternion();
				assets_file <<" quatRotation(<" << boxrot.e0;
				assets_file <<"," << boxrot.e1;
				assets_file <<"," << boxrot.e2;
				assets_file <<"," << boxrot.e3 << ">) \n";
				assets_file <<" translate  <" << myobjshapeasset->GetBoxGeometry().Pos.x;
				assets_file <<"," << myobjshapeasset->GetBoxGeometry().Pos.y;
				assets_file <<"," << myobjshapeasset->GetBoxGeometry().Pos.z << "> \n";

				assets_file <<"}\n"; // end box

				/*
				assets_file <<" pigment {color rgbt <" << 
						myobjshapeasset->GetColor().R << "," << 
						myobjshapeasset->GetColor().G << "," << 
						myobjshapeasset->GetColor().B << "," << 
						myobjshapeasset->GetFading() << "> }\n";
				*/

				assets_file <<"}\n"; // end union
				
				// POV macro - end 
				assets_file << "#end \n";
			}

			// *) asset k of object i is a custom POV ray command ?
			if (k_asset.IsType<ChPovRayAssetCustom>() )
			{
				ChSharedPtr<ChPovRayAssetCustom> myobjcommandasset(k_asset);

				// POV macro to build the asset - begin
				assets_file << "#macro cm_"<< (int) k_asset.get_ptr() << "()\n";

				// add POV custom commands
				assets_file << myobjcommandasset->GetCommands().c_str() << "\n";

				// POV macro - end 
				assets_file << "#end \n";
			}

			// *) asset k of object i is a texture ?
			if (k_asset.IsType<ChTexture>() )
			{
				ChSharedPtr<ChTexture> myobjtextureasset(k_asset);

				// POV macro to build the asset - begin
				assets_file << "#macro cm_"<< (int) k_asset.get_ptr() << "()\n";

				// add POV  texture
				assets_file << "texture { uv_mapping pigment { image_map {";
				if (myobjtextureasset->GetTextureFilename().substr(myobjtextureasset->GetTextureFilename().length()-5,1)==".")
					assets_file << (myobjtextureasset->GetTextureFilename().substr(myobjtextureasset->GetTextureFilename().length()-4,3)).c_str() << " ";
				if (myobjtextureasset->GetTextureFilename().substr(myobjtextureasset->GetTextureFilename().length()-4,1)==".")
					assets_file << (myobjtextureasset->GetTextureFilename().substr(myobjtextureasset->GetTextureFilename().length()-3,3)).c_str() << " ";
				assets_file << "\"" << myobjtextureasset->GetTextureFilename().c_str() << "\"";
				assets_file << " }}}\n";

				// POV macro - end 
				assets_file << "#end \n";
			}

			// *) asset k of object i is a level with sub assets? ?
			if ( k_asset.IsType<ChAssetLevel>() )
			{
				ChSharedPtr<ChAssetLevel> mylevel(k_asset);
				// recurse level...
				//ChFrame<> composedframe = mylevel->GetFrame() >> parentframe;

				std::vector< ChSharedPtr<ChAsset> >& subassetlist = mylevel->GetAssets();
				_recurseExportAssets(subassetlist, assets_file);
			}

		} 

	} // end loop on assets of i-th object 

}

void ChPovRay::ExportAssets()
{
	// open asset file in append mode.
	std::string assets_filename = this->out_script_filename + ".assets";
	ChStreamOutAsciiFile assets_file(assets_filename.c_str(), std::ios::app);

	// This will scan all the ChPhysicsItem added objects, and if
	// they have some reference to renderizable assets, write geoemtries in 
	// the POV assets script.

	for (unsigned int i = 0; i< this->mdata.size(); i++)
	{

		_recurseExportAssets(mdata[i]->GetAssets(), assets_file); 

	} // end loop on objects

}




void ChPovRay::_recurseExportObjData( std::vector< ChSharedPtr<ChAsset> >& assetlist, ChFrame<> parentframe, ChStreamOutAsciiFile& mfilepov)
{
	mfilepov << "union{\n";  // begin union

	// Scan assets in object and write the macro to set their position
	for (unsigned int k = 0; k < assetlist.size(); k++)
	{
		ChSharedPtr<ChAsset> k_asset = assetlist[k];

		// 1) asset k of object i references an .obj wavefront mesh?
		if ( k_asset.IsType<ChObjShapeFile>() ||
			 k_asset.IsType<ChSphereShape>() || 
			 k_asset.IsType<ChBoxShape>() )
		{
			mfilepov << "sh_"<< (unsigned int) k_asset.get_ptr() << "()\n"; // "("; 

			//mfilepov << assetcsys.pos.x << "," << assetcsys.pos.y << "," << assetcsys.pos.z << ",";
			//mfilepov << assetcsys.rot.e0 << "," << assetcsys.rot.e1 << "," << assetcsys.rot.e2 << "," << assetcsys.rot.e3 << ")\n";
		}
		if ( k_asset.IsType<ChPovRayAssetCustom>() || 
			 k_asset.IsType<ChTexture>() )
		{
			mfilepov << "cm_"<< (unsigned int) k_asset.get_ptr() << "()\n";
		}

		if ( k_asset.IsType<ChCamera>() )
		{
			this->camera_found_in_assets = true;
			ChSharedPtr<ChCamera> mycamera(k_asset);
		
			this->camera_location = mycamera->GetPosition() >> parentframe;
			this->camera_aim = mycamera->GetAimPoint() >> parentframe;
			this->camera_up = mycamera->GetUpVector() >> parentframe;
			this->camera_angle = mycamera->GetAngle();
			this->camera_orthographic = mycamera->GetOrthographic();
		}

		if ( k_asset.IsType<ChAssetLevel>() )
		{
			ChSharedPtr<ChAssetLevel> mylevel(k_asset);
			// recurse level...
			ChFrame<> composedframe = mylevel->GetFrame() >> parentframe;
			ChFrame<> subassetframe = mylevel->GetFrame();

			std::vector< ChSharedPtr<ChAsset> >& subassetlist = mylevel->GetAssets();
			_recurseExportObjData(subassetlist, subassetframe, mfilepov);
		}

	} // end loop on assets
	
	// write the rotation and position
	mfilepov <<" quatRotation(<" << parentframe.GetRot().e0;
	mfilepov <<"," << parentframe.GetRot().e1;
	mfilepov <<"," << parentframe.GetRot().e2;
	mfilepov <<"," << parentframe.GetRot().e3 << ">) \n";
	mfilepov <<" translate  <" << parentframe.GetPos().x;
	mfilepov <<"," << parentframe.GetPos().y;
	mfilepov <<"," << parentframe.GetPos().z << "> \n";

	mfilepov << "}\n"; // end union
}


void ChPovRay::ExportData(const std::string &filename)
{
	// Populate the assets (because maybe that during the 
	// animation someone created an object with asset, after
	// the initial call to ExportScript() - but already present
	// assets won't be appended)

	this->ExportAssets();


	// Generate the nnnn.dat and nnnn.pov files:

	try 
	{
		char pathdat[200];
		sprintf(pathdat,"%s.dat", filename.c_str());
		ChStreamOutAsciiFile mfiledat(pathdat);

		char pathpov[200];
		sprintf(pathpov,"%s.pov", filename.c_str());
		ChStreamOutAsciiFile mfilepov(pathpov);

		this->camera_found_in_assets = false;

		// Write custom data commands, if provided by the user
		if ( this->custom_data.size() >0)
		{
			mfilepov << "// Custom user-added script: \n\n";
			mfilepov << this->custom_data;
			mfilepov << "\n\n";
		}

		// Save time-dependent data for the geometry of objects in ...nnnn.POV 
		// and in ...nnnn.DAT file

		for (unsigned int i = 0; i< this->mdata.size(); i++)
		{			
			if (mdata[i].IsType<ChBody>() )
			{
				ChSharedPtr<ChBody> mybody(mdata[i]);

				// Get the current coordinate frame of the i-th object
				ChCoordsys<> assetcsys = CSYSNORM;
				ChFrame<> bodyframe = mybody->GetFrame_REF_to_abs();
				assetcsys = bodyframe.GetCoord();

				// Dump the POV macro that generates the contained asset(s) tree!!!
				_recurseExportObjData(mdata[i]->GetAssets(), bodyframe, mfilepov); 

				// Show body COG?
				if (this->COGs_show)
				{
					ChCoordsys<> cogcsys = mybody->GetFrame_COG_to_abs().GetCoord();
					mfilepov << "sh_csysCOG(";
					mfilepov << cogcsys.pos.x << "," << cogcsys.pos.y << "," << cogcsys.pos.z << ",";
					mfilepov << cogcsys.rot.e0 << "," << cogcsys.rot.e1 << "," << cogcsys.rot.e2 << "," << cogcsys.rot.e3  << ",";
					mfilepov << this->COGs_size << ")\n";
				}
				// Show body frame ref?
				if (this->frames_show)
				{
					mfilepov << "sh_csysFRM(";
					mfilepov << assetcsys.pos.x << "," << assetcsys.pos.y << "," << assetcsys.pos.z << ",";
					mfilepov << assetcsys.rot.e0 << "," << assetcsys.rot.e1 << "," << assetcsys.rot.e2 << "," << assetcsys.rot.e3  << ",";
					mfilepov << this->frames_size << ")\n";
				}
				
			}


			if (mdata[i].IsType<ChParticlesClones>() )
			{
				ChSharedPtr<ChParticlesClones> myclones(mdata[i]);

				// Loop on all particle clones
				for (unsigned int m = 0; m < myclones->GetNparticles() ; ++m)
				{
					// Get the current coordinate frame of the i-th particle
					ChCoordsys<> assetcsys = CSYSNORM;
					assetcsys = myclones->GetParticle(m).GetCoord();

					// Dump the POV macro that generates the contained asset(s) tree!!!
					ChFrame<> particleframe(assetcsys);
					_recurseExportObjData (mdata[i]->GetAssets(), particleframe, mfilepov);


				} // end loop on particles
			}


			if (mdata[i].IsType<ChLinkMateGeneric>() )
			{
				ChSharedPtr<ChLinkMateGeneric> mylinkmate(mdata[i]);
				if (mylinkmate->GetBody1() && mylinkmate->GetBody2() && this->links_show)
				{
					ChFrame<> frAabs = mylinkmate->GetFrame1() >> *mylinkmate->GetBody1();
					ChFrame<> frBabs = mylinkmate->GetFrame2() >> *mylinkmate->GetBody2();
					mfilepov << "sh_csysFRM("; 
					mfilepov << frAabs.GetPos().x << "," << frAabs.GetPos().y << "," << frAabs.GetPos().z << ",";
					mfilepov << frAabs.GetRot().e0 << "," << frAabs.GetRot().e1 << "," << frAabs.GetRot().e2 << "," << frAabs.GetRot().e3 << ",";
					mfilepov << this->links_size*0.7 << ")\n"; // smaller, as 'slave' csys.
					mfilepov << "sh_csysFRM("; 
					mfilepov << frBabs.GetPos().x << "," << frBabs.GetPos().y << "," << frBabs.GetPos().z << ",";
					mfilepov << frBabs.GetRot().e0 << "," << frBabs.GetRot().e1 << "," << frBabs.GetRot().e2 << "," << frBabs.GetRot().e3 << ",";
					mfilepov << this->links_size << ")\n";
				}
			}
			

		} // end loop on objects

		if (this->contacts_show)
		{
			char pathcontacts[200];
			sprintf(pathcontacts,"%s.contacts", filename.c_str());
			ChStreamOutAsciiFile data_contacts(pathcontacts);

			class _reporter_class : public chrono::ChReportContactCallback 
			{
			public:
				virtual bool ReportContactCallback (
								const ChVector<>& pA,				
								const ChVector<>& pB,				
								const ChMatrix33<>& plane_coord,	
								const double& distance,				
								const float& mfriction,			  	
								const ChVector<>& react_forces,		
								const ChVector<>& react_torques,	
								collision::ChCollisionModel* modA,	
								collision::ChCollisionModel* modB) 
				{
					if (fabs(react_forces.x)>1e-8 || fabs(react_forces.y)>1e-8 || fabs(react_forces.z)>1e-8)
					{	
						ChMatrix33<> localmatr(plane_coord);
						ChVector<> n1 = localmatr.Get_A_Xaxis();
						ChVector<> absreac= localmatr * react_forces;
						(*mfile) << pA.x << ", ";
						(*mfile) << pA.y << ", ";
						(*mfile) << pA.z << ", ";
						(*mfile) << n1.x << ", ";
						(*mfile) << n1.y << ", ";
						(*mfile) << n1.z << ", ";
						(*mfile) << absreac.x << ", ";
						(*mfile) << absreac.y << ", ";
						(*mfile) << absreac.z << ", \n";
					}
					return true; // to continue scanning contacts
				}
				// Data 
				ChStreamOutAsciiFile* mfile;
			};

			_reporter_class my_contact_reporter;
			my_contact_reporter.mfile = &data_contacts;

			// scan all contacts 
			this->mSystem->GetContactContainer()->ReportAllContacts(&my_contact_reporter);
		}

		// If a camera have been found in assets, create it and override the default one
		if (this->camera_found_in_assets)
		{
			mfilepov <<	"camera { \n";   
			if (camera_orthographic) 
			{
					mfilepov << " orthographic \n";
					mfilepov <<	" right x * " << (camera_location - camera_aim).Length() << " * tan ((( " << camera_angle << " *0.5)/180)*3.14) \n";
					mfilepov <<	" up y * image_height/image_width * " << (camera_location - camera_aim).Length() << " * tan (((" << camera_angle << "*0.5)/180)*3.14) \n";
					ChVector<> mdir = (camera_aim - camera_location)*0.00001;
					mfilepov <<	" direction <" << mdir.x << "," << mdir.y << "," << mdir.z << "> \n";
			}
			else
			{
					mfilepov <<	" right -x*image_width/image_height \n";
					mfilepov <<	" angle " << camera_angle << " \n";
			}
					mfilepov << " location <" << camera_location.x <<","<<camera_location.y <<","<< camera_location.z <<"> \n" <<
						" look_at <" << camera_aim.x <<","<<camera_aim.y <<","<<camera_aim.z <<"> \n" << 
						" sky <" << camera_up.x <<","<<camera_up.y <<","<<camera_up.z <<"> \n";
			mfilepov <<	"}\n\n\n"; 
		}

	}
	catch (ChException)
	{
		char error[400];
		sprintf(error,"Can't save data into file %s.pov (or .dat)", filename.c_str() );
		throw (ChException(error));
	}

	// Increment the number of the frame.
	this->framenumber ++;
}




