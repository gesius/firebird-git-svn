/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2008 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "consts_pub.h"
#include "iberror.h"
#include "../yvalve/PluginManager.h"
#include "../jrd/ErrorImpl.h"
#include "../common/os/path_utils.h"
#include "../jrd/err_proto.h"
#include "../common/isc_proto.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/init.h"
#include "../common/config/config.h"
#include "../common/config/config_file.h"
#include "../common/utils_proto.h"
#include "../common/ScanDir.h"
#include "../common/classes/GenericMap.h"
#include "../common/db_alias.h"

//#define DEBUG_PLUGINS

namespace Firebird {


namespace
{
	void splitWord(PathName& to, PathName& list)
	{
		list.alltrim(" \t");
		PathName::size_type p = list.find_first_of(" \t,;");
		if (p == PathName::npos)
		{
			if (list.isEmpty())
			{
				to = "";
				return;
			}
			to = list;
			list = "";
			return;
		}

		to = list.substr(0, p);
		list = list.substr(p);
		list.ltrim(" \t,;");
	}

	void extension(PathName& file, const char* newExt)
	{
		PathName::size_type p = file.rfind(PathUtils::dir_sep);
		if (p == PathName::npos)
		{
			p = 0;
		}
		p = file.find('.', p);
		if (p == PathName::npos)
		{
			file += '.';
		}
		else
		{
			file.erase(p + 1);
		}

		file += newExt;
	}

	class StaticConfHolder
	{
	public:
		StaticConfHolder(MemoryPool&)
			: confFile(FB_NEW(*getDefaultMemoryPool()) ConfigFile(*getDefaultMemoryPool(),
				fb_utils::getPrefix(fb_utils::FB_DIR_CONF, "plugins.conf"), ConfigFile::HAS_SUB_CONF))
		{
		}

		ConfigFile* get()
		{
			return confFile;
		}

	private:
		RefPtr<ConfigFile> confFile;
	};
	GlobalPtr<StaticConfHolder> pluginsConf;

	RefPtr<ConfigFile> findConfig(const char* param, const char* pluginName)
	{
		ConfigFile* f = pluginsConf->get();
		if (f)
		{
			const ConfigFile::Parameter* plugPar = f->findParameter(param, pluginName);
			if (plugPar && plugPar->sub.hasData())
			{
				return plugPar->sub;
			}
		}

		return RefPtr<ConfigFile>(NULL);
	}

	bool flShutdown = false;

	class ConfigParameterAccess : public StdIface<IConfigParameter, FB_I_CONFIG_PARAMETER_VERSION>
	{
	public:
		ConfigParameterAccess(Interface* c, const ConfigFile::Parameter* p) : cf(c), par(p) { }

		const char* FB_CARG name()
		{
			return par ? par->name.c_str() : NULL;
		}

		const char* FB_CARG value()
		{
			return par ? par->value.nullStr() : NULL;
		}

		IConfig* FB_CARG sub();

		int FB_CARG release()
		{
			if (--refCounter == 0)
			{
				delete this;
				return 0;
			}

			return 1;
		}

	private:
		RefPtr<Interface> cf;
		const ConfigFile::Parameter* par;
	};

	class ConfigAccess : public StdIface<IConfig, FB_I_CONFIG_VERSION>
	{
	public:
		ConfigAccess(RefPtr<ConfigFile> c) : confFile(c) { }

		IConfigParameter* FB_CARG find(const char* name)
		{
			return confFile.hasData() ? newParam(confFile->findParameter(name)) : NULL;
		}

		IConfigParameter* FB_CARG findValue(const char* name, const char* value)
		{
			return confFile.hasData() ? newParam(confFile->findParameter(name, value)) : NULL;
		}

		IConfigParameter* FB_CARG findPos(const char* name, unsigned int n)
		{
			if (!confFile.hasData())
			{
				return NULL;
			}

			const ConfigFile::Parameters& p = confFile->getParameters();
			size_t pos;
			if (!p.find(name, pos))
			{
				return NULL;
			}

			if (n + pos >= p.getCount() || p[n + pos].name != name)
			{
				return NULL;
			}

			return newParam(&p[n + pos]);
		}

		int FB_CARG release()
		{
			if (--refCounter == 0)
			{
				delete this;
				return 0;
			}

			return 1;
		}

	private:
		RefPtr<ConfigFile> confFile;

		IConfigParameter* newParam(const ConfigFile::Parameter* p)
		{
			if (p)
			{
				return new ConfigParameterAccess(this, p);
			}

			return NULL;
		}
	};

	IConfig* ConfigParameterAccess::sub()
	{
		if (par && par->sub.hasData())
		{
			return new ConfigAccess(par->sub);
		}

		return NULL;
	}

	struct RegisteredPlugin		// This is POD object
	{
		RegisteredPlugin(PluginsFactory* f, const char* nm, unsigned int t)
			: factory(f), name(nm), type(t)
		{ }

		RegisteredPlugin()
			: factory(NULL), name(NULL), type(0)
		{ }

		PluginsFactory* factory;
		const char* name;
		unsigned int type;
	};

	class PluginModule : public Firebird::RefCounted, public GlobalStorage
	{
	public:
		PluginModule(ModuleLoader::Module* pmodule, const PathName& pname);

		unsigned int addPlugin(const RegisteredPlugin& p)
		{
			regPlugins.add(p);
			return regPlugins.getCount() - 1;
		}

		int findPlugin(unsigned int type, const PathName& name)
		{
			// typically modules do not contain too many plugins
			// therefore direct array scan is OK here
			for (unsigned int i = 0; i < regPlugins.getCount(); ++i)
			{
				if (type == regPlugins[i].type && name == regPlugins[i].name)
				{
					return i;
				}
			}

			return -1;
		}

		RegisteredPlugin& getPlugin(unsigned int i)
		{
			return regPlugins[i];
		}

		PluginModule* findModule(const PathName& pname)
		{
			if (name == pname)
			{
				return this;
			}
			if (next)
			{
				return next->findModule(pname);
			}
			return NULL;
		}

		const char* getName() const
		{
			return name.nullStr();
		}

		void setCleanup(IModuleCleanup* c)
		{
			cleanup = c;
		}

	private:
		~PluginModule()
		{
			if (next)
			{
				next->prev = prev;
			}
			*prev = next;

			for (unsigned int i = 0; i < regPlugins.getCount(); ++i)
			{
				regPlugins[i].factory->release();
			}

			if (cleanup.hasData())
			{
				cleanup->doClean();
			}
		}

		Firebird::AutoPtr<ModuleLoader::Module> module;
		Firebird::RefPtr<Firebird::IModuleCleanup> cleanup;
		HalfStaticArray<RegisteredPlugin, 2> regPlugins;
		PluginModule* next;
		PluginModule** prev;
		PathName name;
	};

	class ConfiguredPlugin : public RefCounted, public GlobalStorage
	{
	public:
		ConfiguredPlugin(RefPtr<PluginModule> pmodule, unsigned int preg,
						 RefPtr<ConfigFile> pconfig, const PathName& pconfName,
						 const PathName& pplugName)
			: module(pmodule), regPlugin(preg), defaultConfig(pconfig),
			  confName(getPool(), pconfName), plugName(getPool(), pplugName)
		{
			if (defaultConfig.hasData())
			{
				const ConfigFile::Parameter* p = defaultConfig->findParameter("ConfigFile");
				if (p && p->value.hasData())
				{
					confName = p->value.ToPathName();
				}
			}
#ifdef DEBUG_PLUGINS
			RegisteredPlugin& r(module->getPlugin(regPlugin));
			fprintf(stderr, " ConfiguredPlugin %s module %s registered as %s type %d order %d\n",
					plugName.c_str(), module->getName(), r.name, r.type, regPlugin);
#endif
		}

		const char* getConfigFileName()
		{
			return confName.c_str();
		}

		IConfig* getDefaultConfig()
		{
			if (defaultConfig.hasData())
			{
				const ConfigFile::Parameter* p = defaultConfig->findParameter("Config");
				return new ConfigAccess(p ? findConfig("Config", p->value.c_str()) : RefPtr<ConfigFile>(NULL));
			}

			PluginInterface pi;
			IConfig* iconf = pi->getConfig(confName.nullStr());

			return iconf;
		}

		const PluginModule* getModule() const
		{
			return module;
		}

		Plugin* factory(IFirebirdConf *iFirebirdConf);

		~ConfiguredPlugin();

	private:
		RefPtr<PluginModule> module;
		unsigned int regPlugin;
		RefPtr<ConfigFile> defaultConfig;
		PathName confName;
		PathName plugName;
	};

	class FactoryParameter : public StdIface<IFactoryParameter, FB_FACTORY_PARAMETER_VERSION>
	{
	public:
		FactoryParameter(ConfiguredPlugin* cp, IFirebirdConf* fc)
			: configuredPlugin(cp), firebirdConf(fc)
		{ }

		const char* FB_CARG getConfigFileName()
		{
			return configuredPlugin->getConfigFileName();
		}

		IConfig* FB_CARG getDefaultConfig()
		{
			return configuredPlugin->getDefaultConfig();
		}

		IFirebirdConf* FB_CARG getFirebirdConf()
		{
			if (!firebirdConf.hasData())
			{
				RefPtr<Config> specificConf(Config::getDefaultConfig());
				firebirdConf = new FirebirdConf(specificConf);
			}

			firebirdConf->addRef();
			return firebirdConf;
		}

		int FB_CARG release()
		{
			if (--refCounter == 0)
			{
				delete this;
				return 0;
			}

			return 1;
		}

	private:
		RefPtr<ConfiguredPlugin> configuredPlugin;
		RefPtr<IFirebirdConf> firebirdConf;
	};

	Plugin* ConfiguredPlugin::factory(IFirebirdConf *firebirdConf)
	{
		FactoryParameter* par = new FactoryParameter(this, firebirdConf);
		Plugin* plugin = module->getPlugin(regPlugin).factory->createPlugin(par);
		if (plugin)
		{
			plugin->owner(par);
		}
		return plugin;
	}


	class MapKey : public AutoStorage
	{
		public:
			MapKey(unsigned int ptype, const PathName& pname)
				: type(ptype), name(getPool(), pname)
			{ }

			MapKey(MemoryPool& p, const MapKey& mk)
				: AutoStorage(p), type(mk.type), name(getPool(), mk.name)
			{ }

			bool operator<(const MapKey& c) const	{	return type < c.type || (type == c.type && name < c.name);	}
			bool operator==(const MapKey& c) const	{	return type == c.type && name == c.name;					}
			bool operator>(const MapKey& c) const	{	return type > c.type || (type == c.type && name > c.name);	}
		private:
			unsigned int type;
			PathName name;
	};

	static bool destroyingPluginsMap = false;

	class PluginsMap : public GenericMap<Pair<Left<MapKey, ConfiguredPlugin*> > >
	{
	public:
		explicit PluginsMap(MemoryPool& p)
			: GenericMap<Pair<Left<MapKey, ConfiguredPlugin*> > >(p)
		{
		}

		~PluginsMap()
		{
			MutexLockGuard g(mutex);

			destroyingPluginsMap = true;
			// unload plugins
			Accessor accessor(this);
			for (bool found = accessor.getFirst(); found; found = accessor.getNext())
			{
				ConfiguredPlugin* plugin = accessor.current()->second;
				plugin->release();
			}
		}

		Mutex mutex;
	};

	GlobalPtr<PluginsMap> plugins;

	ConfiguredPlugin::~ConfiguredPlugin()
	{
		if (!destroyingPluginsMap)
		{
			plugins->remove(MapKey(module->getPlugin(regPlugin).type, plugName));
		}
#ifdef DEBUG_PLUGINS
		fprintf(stderr, "~ConfiguredPlugin %s type %d\n", plugName.c_str(), module->getPlugin(regPlugin).type);
#endif
	}

	PluginModule* modules = NULL;

	PluginModule* builtin = NULL;
	PluginModule* current = NULL;

	PluginModule::PluginModule(ModuleLoader::Module* pmodule, const PathName& pname)
			: module(pmodule), regPlugins(getPool()), next(modules),
			  prev(&modules), name(getPool(), pname)
	{
		if (next)
		{
			next->prev = &next;
		}
		*prev = this;
	}

	class PluginSet : public StdIface<IPluginSet, FB_PLUGIN_SET_VERSION>
	{
	public:
		const char* FB_CARG name() const
		{
			return currentPlugin.hasData() ? currentName.c_str() : NULL;
		}

		const char* FB_CARG module() const
		{
			return currentPlugin.hasData() ? currentPlugin->getModule()->getName() : NULL;
		}

		void FB_CARG set(const char* newName)
		{
			namesList = newName;
			namesList.alltrim(" \t");
			next();
		}

		Plugin* FB_CARG plugin();
		void FB_CARG next();

		PluginSet(unsigned int pinterfaceType, const char* pnamesList,
				  int pdesiredVersion, void* pmissingFunctionClass,
				  IFirebirdConf* fbConf)
			: interfaceType(pinterfaceType), namesList(getPool()),
			  desiredVersion(pdesiredVersion), missingFunctionClass(pmissingFunctionClass),
			  currentName(getPool()), currentPlugin(NULL),
			  firebirdConf(fbConf), masterInterface(fb_get_master_interface())
		{
			namesList.assign(pnamesList);
			namesList.alltrim(" \t");
			next();
		}

		int FB_CARG release()
		{
			if (--refCounter == 0)
			{
				delete this;
				return 0;
			}

			return 1;
		}

	private:
		unsigned int interfaceType;
		PathName namesList;
		int desiredVersion;
		void* missingFunctionClass;

		PathName currentName;
		RefPtr<ConfiguredPlugin> currentPlugin;		// Missing data in this field indicates EOF

		IFirebirdConf* firebirdConf;
		AutoPtr<IMaster, AutoInterface> masterInterface;

		RefPtr<PluginModule> loadModule(const PathName& modName);

		void loadError(const Arg::StatusVector& error)
		{
			iscLogStatus("PluginSet", error.value());
			error.raise();
		}
	};

	// ************************************* //
	// ** next() - core of plugin manager ** //
	// ************************************* //
	void FB_CARG PluginSet::next()
	{
		if (currentPlugin.hasData())
		{
			currentPlugin = NULL;
		}

		MutexLockGuard g(plugins->mutex);

		while (namesList.hasData())
		{
			splitWord(currentName, namesList);

			// First check - may be currentName is present among already configured plugins
			ConfiguredPlugin* tmp = NULL;
			if (plugins->get(MapKey(interfaceType, currentName), tmp))
			{
				currentPlugin = tmp;
				break;
			}

			// define default values for plugin ...
			PathName curModule = fb_utils::getPrefix(fb_utils::FB_DIR_PLUGINS, currentName.c_str());
			PathName regName = currentName;

			// and try to load them from conf file
			RefPtr<ConfigFile> conf = findConfig("Plugin", currentName.c_str());

			if (conf.hasData())
			{
				const ConfigFile::Parameter* v = conf->findParameter("RegisterName");
				if (v)
				{
					regName = v->value.ToPathName();
				}

				v = conf->findParameter("Module");
				if (v)
				{
					curModule = v->value.ToPathName();
				}
			}

			// Check if module is loaded and load it if needed
			RefPtr<PluginModule> m(modules->findModule(curModule));
			if (!m.hasData() && !flShutdown)
			{
				m = loadModule(curModule);
			}
			if (!m.hasData())
			{
				continue;
			}

			int r = m->findPlugin(interfaceType, regName);
			if (r < 0)
			{
				gds__log("Misconfigured: module %s does not contain plugin %s type %d",
						 curModule.c_str(), regName.c_str(), interfaceType);
				continue;
			}

			PathName plugConfigFile = curModule;
			extension(plugConfigFile, "conf");

			currentPlugin = new ConfiguredPlugin(m, r, conf, plugConfigFile, currentName);

			plugins->put(MapKey(interfaceType, currentName), currentPlugin);
			return;
		}
	}

	RefPtr<PluginModule> PluginSet::loadModule(const PathName& asIsModuleName)
	{
		PathName fixedModuleName(asIsModuleName);

		ModuleLoader::Module* module = ModuleLoader::loadModule(fixedModuleName);

		if (!module && !ModuleLoader::isLoadableModule(fixedModuleName))
		{
			ModuleLoader::doctorModuleExtension(fixedModuleName);
			module = ModuleLoader::loadModule(fixedModuleName);
		}

		if (!module)
		{
			if (ModuleLoader::isLoadableModule(fixedModuleName))
			{
				loadError(Arg::Gds(isc_pman_cannot_load_plugin) << fixedModuleName);
			}

			return RefPtr<PluginModule>(NULL);
		}

		RefPtr<PluginModule> rc(new PluginModule(module, asIsModuleName));
		StartLoadedModule* startModule;
		if (module->findSymbol(STRINGIZE(FB_PLUGIN_ENTRY_POINT), startModule))
		{
			current = rc;
			startModule(masterInterface);
			current = NULL;
			return rc;
		}

		loadError(Arg::Gds(isc_pman_entrypoint_notfound) << fixedModuleName);
		return RefPtr<PluginModule>(NULL);	// compiler warning silencer
	}

	Plugin* FB_CARG PluginSet::plugin()
	{
		while (currentPlugin.hasData())
		{
			Plugin* p = currentPlugin->factory(firebirdConf);
			if (p)
			{
				if (masterInterface->upgradeInterface(p, desiredVersion, missingFunctionClass) >= 0)
				{
					return p;
				}

				PluginInterface pi;
				pi->releasePlugin(p);
			}
			next();
		}
		//currentPlugin->addRef();

		return NULL;
	}
}


PluginManager::PluginManager()
{
	MutexLockGuard g(plugins->mutex);

	if (!builtin)
	{
		builtin = new PluginModule(NULL, "<builtin>");
		builtin->addRef();	// Will never be unloaded
		current = builtin;
	}
}


void FB_CARG PluginManager::registerPlugin(unsigned int interfaceType, const char* defaultName, PluginsFactory* factory)
{
	MutexLockGuard g(plugins->mutex);

	if (!current)
	{
		// not good time to call this function - ignore request
		gds__log("Unexpected call to register plugin %s, type %d - ignored\n", defaultName, interfaceType);
		return;
	}

	unsigned int r = current->addPlugin(RegisteredPlugin(factory, defaultName, interfaceType));

	if (current == builtin)
	{
		PathName plugConfigFile = fb_utils::getPrefix(fb_utils::FB_DIR_PLUGINS, defaultName);
		extension(plugConfigFile, "conf");

		ConfiguredPlugin* p = new ConfiguredPlugin(RefPtr<PluginModule>(builtin), r,
									findConfig("Plugin", defaultName), plugConfigFile, defaultName);
		p->addRef();  // Will never be unloaded
		plugins->put(MapKey(interfaceType, defaultName), p);
	}
}


void FB_CARG PluginManager::setModuleCleanup(IModuleCleanup* cleanup)
{
	MutexLockGuard g(plugins->mutex);

	if (!current)
	{
		// not good time to call this function - ignore request
		gds__log("Unexpected call to set module cleanup - ignored\n");
		return;
	}

	current->setCleanup(cleanup);
}


IPluginSet* FB_CARG PluginManager::getPlugins(unsigned int interfaceType, const char* namesList,
											  int desiredVersion, void* missingFunctionClass,
											  IFirebirdConf* firebirdConf)
{
	MutexLockGuard g(plugins->mutex);

	return new PluginSet(interfaceType, namesList, desiredVersion, 
						 missingFunctionClass, firebirdConf);
}


void FB_CARG PluginManager::releasePlugin(Plugin* plugin)
{
	MutexLockGuard g(plugins->mutex);

	Interface* parent = plugin->owner(NULL);

	if (plugin->release() == 0)
	{
		fb_assert(parent);
		if (parent)
		{
			parent->release();
		}
	}
	else {
		plugin->owner(parent);
	}
}


IConfig* FB_CARG PluginManager::getConfig(const char* filename)
{
	return new ConfigAccess(RefPtr<ConfigFile>(FB_NEW(*getDefaultMemoryPool()) ConfigFile(filename, 0)));
}


void PluginManager::shutdown()
{
	flShutdown = true;
}

}	// namespace Firebird
