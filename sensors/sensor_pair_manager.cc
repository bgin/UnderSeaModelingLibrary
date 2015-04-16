/**
 * @file sensor_pair_manager.cc
 * Manages the containers for all the sensor pair's in use by the USML.
 */
#include <usml/sensors/sensor_pair_manager.h>
#include <usml/sensors/sensor_manager.h>
#include <boost/foreach.hpp>

using namespace usml::sensors;

/**
 * Initialization of private static member _instance
 */
unique_ptr<sensor_pair_manager> sensor_pair_manager::_instance;

/**
 * The _mutex for the singleton sensor_pair_manager.
 */
read_write_lock sensor_pair_manager::_instance_mutex;

/**
 * Singleton Constructor - Double Check Locking Pattern DCLP
 */
sensor_pair_manager* sensor_pair_manager::instance() {
	sensor_pair_manager* tmp = _instance.get();
	if (tmp == NULL) {
		write_lock_guard guard(_instance_mutex);
		tmp = _instance.get();
		if (tmp == NULL) {
			tmp = new sensor_pair_manager();
			_instance.reset(tmp);
		}
	}
	return tmp;
}

/**
 * Default destructor.
 */
sensor_pair_manager::~sensor_pair_manager() {

    // Remove all sensor_pair pointers from the _map
    sensor_map_template<std::string, sensor_pair*>::iterator iter;
    for ( iter = _map.begin(); iter != _map.end(); ++iter )
    {
        sensor_pair* pair_data = iter->second;
        #ifdef USML_DEBUG
            cout << "  ~sensor_pair_manager: deleting sensor_pair " << pair_data << endl;
        #endif
        delete pair_data;
    }
}

/**
 * Reset the sensor_pair_manager instance to empty.
 */
void sensor_pair_manager::reset() {
    write_lock_guard(_instance_mutex);
    _instance.reset();
}

/**
 * Finds all the keys in the _maps that are in the sensor_query_map
 */
std::set<std::string> sensor_pair_manager::find_pairs(sensor_query_map& sensors)
{   
    std::set<std::string> hash_keys;
    std::set<sensor_model::id_type> source_ids;
    std::set<sensor_model::id_type> receiver_ids;
    std::set<sensor_model::id_type>::iterator test_iter;

    // Create a source_keys list and a receiver_key list 
    // of the requested items
    xmitRcvModeType mode;
    sensor_model::id_type sensorID;
    std::pair<sensor_model::id_type, xmitRcvModeType> p;
    BOOST_FOREACH(p, sensors)
    {
        sensorID = p.first;
        mode = p.second;
      
        // Only add keys if the sensorID already exist in its respected list
        switch ( mode )
        {
            case usml::sensors::SOURCE:
                test_iter = _src_list.find(sensorID);
                if ( test_iter != _src_list.end() ) {
                    source_ids.insert(sensorID);
                }
                break;
            case usml::sensors::RECEIVER:
                test_iter = _rcv_list.find(sensorID);
                if ( test_iter != _rcv_list.end() ) {
                    receiver_ids.insert(sensorID);
                }
                break;
            case usml::sensors::BOTH:
                test_iter = _src_list.find(sensorID);
                if ( test_iter != _src_list.end() )
                {
                    source_ids.insert(sensorID);
               
                    test_iter = _rcv_list.find(sensorID);
                    if ( test_iter != _rcv_list.end() ) {
                        receiver_ids.insert(sensorID);
                    } else { // Did not exist, backout of source_ids
                        source_ids.erase(sensorID);
                    }
                }
                break;
            default:
                break;
        }
    }

    // Build hash_keys from source_ids and receiver_ids 
    BOOST_FOREACH(sensor_model::id_type srcID, source_ids)
    {
        BOOST_FOREACH(sensor_model::id_type rcvID, receiver_ids)
        {
            std::string key = generate_hash_key(srcID, rcvID);
            hash_keys.insert(key);
        }
    }
    
    return hash_keys;
}

/**
 * Gets the fathometers for the query of sensors provided
 */
fathometer_model::fathometer_package sensor_pair_manager::get_fathometers(sensor_query_map sensors)
{
    sensor_pair* pair;
    read_lock_guard guard(_manager_mutex);

    std::set<std::string> keys = find_pairs(sensors);

    sensor_model::id_type src_id, rcv_id;
    fathometer_model::fathometer_package fathometers;
    fathometers.reserve(keys.size());
    fathometer_model* fathometer; 
    BOOST_FOREACH(std::string s, keys)
    {
        pair = _map.find(s);
        sensor_pair* pair_data = pair;
        if ( pair_data != NULL ) {
            shared_ptr<eigenray_list> eigenrays = pair_data->eigenrays();
            if (eigenrays.get() != NULL){
                src_id = pair_data->source()->sensorID();
                rcv_id = pair_data->receiver()->sensorID();
                wposition1 src_pos = pair_data->source()->position();
                wposition1 rcv_pos = pair_data->receiver()->position();
                fathometer = new fathometer_model(src_id, rcv_id, src_pos, rcv_pos, eigenrays);
                #ifdef USML_DEBUG
                    cout << "sensor_pair_manager: get_fathometers - added fathometer for pair "
                        << "src " << src_id << " rcv " << rcv_id << endl;
                #endif
                fathometers.push_back(fathometer);
            }
        }
    }
    return fathometers;
}

/**
 * Builds new sensor_pair objects in reaction to notification
 * that a sensor is being added.
 */
void sensor_pair_manager::add_sensor(sensor_model* sensor) {
	write_lock_guard guard(_manager_mutex);
	#ifdef USML_DEBUG
		cout << "sensor_pair_manager: add sensor("
		<< sensor->sensorID() << ")" << endl;
	#endif

	// add sensorID to the lists of active sources and receivers

	switch (sensor->mode()) {
	case usml::sensors::SOURCE:
		_src_list.insert(sensor->sensorID());
		break;
	case usml::sensors::RECEIVER:
		_rcv_list.insert(sensor->sensorID());
		break;
	case usml::sensors::BOTH:
		_src_list.insert(sensor->sensorID());
		_rcv_list.insert(sensor->sensorID());
		break;
	default:
		break;
	}
    
    // Add pair as required

    switch ( sensor->mode() )
    {
        case usml::sensors::SOURCE:
            add_multistatic_source(sensor);
            break;
        case usml::sensors::RECEIVER:
            add_multistatic_receiver(sensor);
            break;
        case usml::sensors::BOTH:
            // Check frequency band overlap
            add_monostatic_pair(sensor);

            // add multistatic pairs when multistatic is true 
            if ( sensor->source()->multistatic() ) {
                add_multistatic_source(sensor);    
            }              
            if ( sensor->receiver()->multistatic() ) {
                add_multistatic_receiver(sensor);
            }
            break;
        default:
            break;
    }
    #ifdef USML_DEBUG
        // Print out all pairs
        cout << "sensor_pair_manager:  current pairs" << endl;
        sensor_map_template<std::string, sensor_pair*>::iterator iter;
        for ( iter = _map.begin(); iter != _map.end(); ++iter )
        {
            std::string key  = iter->first;
            cout << "     pair  src_rcv " << key << endl;      
         } 
    #endif
}

/**
 * Removes existing sensor_pair object in reaction to notification
 * that the sensor is about to be deleted.
 */
bool sensor_pair_manager::remove_sensor(sensor_model* sensor) {
    size_t result = 0;
	write_lock_guard guard(_manager_mutex);
	#ifdef USML_DEBUG
		cout << "sensor_pair_manager: remove sensor("
		<< sensor->sensorID() << ")" << endl;
	#endif

	// remove sensorID from the lists of active sources and receivers

	switch (sensor->mode()) {
	case usml::sensors::SOURCE:
        result = _src_list.erase(sensor->sensorID());
		break;
	case usml::sensors::RECEIVER:
        result = _rcv_list.erase(sensor->sensorID());
		break;
	case usml::sensors::BOTH:
        result =_src_list.erase(sensor->sensorID());
        result = _rcv_list.erase(sensor->sensorID());
		break;
	default:
		break;
	}
    // Exit if the sensorID/mode was not found
    if ((int) result == 0 ) return false;

	// Remove pair as required

    switch ( sensor->mode() )
    {
        case usml::sensors::SOURCE:
            remove_multistatic_source(sensor);
            break;
        case usml::sensors::RECEIVER:
            remove_multistatic_receiver(sensor);
            break;
        case usml::sensors::BOTH:
            // Check frequency band overlap
            remove_monostatic_pair(sensor);

            // add multistatic pairs when multistatic is true 
            if ( sensor->source()->multistatic() )
            {
                remove_multistatic_source(sensor);
            }
            if ( sensor->receiver()->multistatic() )
            {
                remove_multistatic_receiver(sensor);
            }
            break;
        default:
            break;
    }
    return true;
}

/**
 * Utility to build a monostatic pair
 */
void sensor_pair_manager::add_monostatic_pair(sensor_model* sensor) {
	sensor_model::id_type sourceID = sensor->sensorID();
    std::string hash_key = generate_hash_key(sourceID, sourceID);
	sensor_pair* pair = new sensor_pair(sensor, sensor);
	_map.insert(hash_key, pair);
	sensor->add_sensor_listener(pair);
	#ifdef USML_DEBUG
		cout << "   add_monostatic_pair: sensor_pair("
		<< sourceID << "," << sourceID << ")" << endl;
	#endif
}

/**
 * Utility to build a multistatic pair from the source.
 */
void sensor_pair_manager::add_multistatic_source(sensor_model* source) {
	sensor_model::id_type sourceID = source->sensorID();
	BOOST_FOREACH( sensor_model::id_type receiverID, _rcv_list ) {
		if ( sourceID != receiverID ) {
            sensor_model* receiver_sensor = sensor_manager::instance()->find(receiverID);
            if ( receiver_sensor != NULL &&
                    receiver_sensor->receiver()->multistatic() )
			{
                std::string hash_key = generate_hash_key(sourceID, receiverID);
                sensor_pair* pair = new sensor_pair(source, receiver_sensor);
                _map.insert(hash_key, pair);
				source->add_sensor_listener(pair);
                receiver_sensor->add_sensor_listener(pair);
				#ifdef USML_DEBUG
					cout << "   add_multistatic_source: sensor_pair("
					<< sourceID << "," << receiverID << ")" << endl;
				#endif
			}
		}
	}
}

/**
 * Utility to build a multistatic pair from the receiver.
 */
void sensor_pair_manager::add_multistatic_receiver(sensor_model* receiver) {
	sensor_model::id_type receiverID = receiver->sensorID();
	BOOST_FOREACH( sensor_model::id_type sourceID, _src_list ) {
		if ( sourceID != receiverID ) { // exclude monostatic case
			sensor_model* source_sensor = sensor_manager::instance()->find(sourceID);
            if ( source_sensor != NULL && 
                    source_sensor->source()->multistatic() )
			{
                std::string hash_key = generate_hash_key(sourceID, receiverID);
                sensor_pair* pair = new sensor_pair(source_sensor, receiver);
                _map.insert(hash_key, pair);
                source_sensor->add_sensor_listener(pair);
				receiver->add_sensor_listener(pair);
				#ifdef USML_DEBUG
					cout << "   add_multistatic_receiver: sensor_pair("
					<< sourceID << "," << receiverID << ")" << endl;
				#endif
			}
		}
	}
}

/**
 * Utility to remove a monostatic pair
 */
void sensor_pair_manager::remove_monostatic_pair(sensor_model* sensor) {
	sensor_model::id_type sourceID = sensor->sensorID();
    std::string hash_key = generate_hash_key(sourceID, sourceID);
	sensor_pair* pair = _map.find(hash_key);
	if (pair != NULL) {
		sensor->remove_sensor_listener(pair);
        delete pair;
		_map.erase(hash_key);
		#ifdef USML_DEBUG
			cout << "   remove_monostatic_pair: sensor_pair("
				 << sourceID << "," << sourceID << ")" << endl;
		#endif
	}
}

/**
 * Utility to remove a multistatic pair from the source.
 */
void sensor_pair_manager::remove_multistatic_source(sensor_model* source) {
	sensor_model::id_type sourceID = source->sensorID();
	BOOST_FOREACH( sensor_model::id_type receiverID, _rcv_list ) {
		if ( sourceID != receiverID ) {
            sensor_model* receiver_sensor = sensor_manager::instance()->find(receiverID);
            if ( receiver_sensor != NULL &&
                    receiver_sensor->receiver()->multistatic() )
			{
                std::string hash_key = generate_hash_key(sourceID, receiverID);
				sensor_pair* pair = _map.find(hash_key);
				if ( pair != NULL ) {
					source->remove_sensor_listener(pair);
                    receiver_sensor->remove_sensor_listener(pair);
                    delete pair;
					_map.erase( hash_key );
					#ifdef USML_DEBUG
						cout << "   remove_multistatic_source: sensor_pair("
							 << sourceID << "," << receiverID << ")" << endl;
					#endif
				}
			}
		}
	}
}

/**
 * Utility to remove a multistatic pair from the receiver.
 */
void sensor_pair_manager::remove_multistatic_receiver(sensor_model* receiver) {
	sensor_model::id_type receiverID = receiver->sensorID();
	BOOST_FOREACH( sensor_model::id_type sourceID, _src_list ) {
		if ( sourceID != receiverID ) { // exclude monostatic case
			sensor_model* source_sensor = sensor_manager::instance()->find(sourceID);
            if ( source_sensor != NULL && 
                source_sensor->source()->multistatic() )
			{
                std::string hash_key = generate_hash_key(sourceID, receiverID);
				sensor_pair* pair = _map.find(hash_key);
				if ( pair != NULL ) {
                    source_sensor->remove_sensor_listener(pair);
					receiver->remove_sensor_listener(pair);
                    delete pair;
					_map.erase( hash_key );
					#ifdef USML_DEBUG
						cout << "   remove_multistatic_receiver: sensor_pair("
							 << sourceID << "," << receiverID << ")" << endl;
					#endif
				}
			}
		}
	}
}
