/**
 * @file eigenverb_monostatic.cc
 */

#include <usml/waveq3d/eigenverb_monostatic.h>

//#define COLLISION_DEBUG

using namespace usml::waveq3d ;

/**  Constructor  **/
eigenverb_monostatic::eigenverb_monostatic( ocean_model& ocean,
    wave_queue_reverb& wave, double pulse, unsigned num_bins, double max_time )
{
        // Set all local member variables for use in calculations
    _pulse = pulse ;
    _max_index = num_bins-1 ;
    _max_time = max_time ;
        // extract pointers to various spreading/boundary model(s)
    _spreading_model = wave.getSpreading_Model() ;
    _bottom_boundary = &ocean.bottom() ;
    _surface_boundary = &ocean.surface() ;
    _volume_boundary = ocean.volume() ;
    unsigned _n = ocean.volume()->getNumberOfLayers() ;
    _upper.resize( _n ) ;
    _lower.resize( _n ) ;
        // Grab this wave's ID, used to determine volume layer interactions
    _source_origin = wave.getID() ;
        // Initialize all the memory of _reverberation_curve
    _reverberation_curve.resize( num_bins ) ;
    for(vector<double>::iterator i=_reverberation_curve.begin();
            i!=_reverberation_curve.end(); ++i) {
        (*i) = 1e-20 ;
    }
}

/**
 * Places an eigenverb into the class of "upper" bins to be used for the overall
 * reverberation calculation.
 */
void eigenverb_monostatic::notifyUpperCollision( unsigned de, unsigned az, double time,
               double dt, double grazing, double speed, const seq_vector& frequencies,
               const wposition1& position, const wvector1& ndirection,
               const vector<double>& boundary_loss, unsigned ID )
{
    #ifdef COLLISION_DEBUG
        std::cout << "**** Entering eigenverb_monostatic::notifyUpperCollision" << std::endl ;
        std::cout << "de: " << de << " az: " << az << " time: " << time << std::endl ;
        std::cout << "grazing: " << grazing << " ID: " << ID << std::endl ;
    #endif
    eigenverb verb ;
    create_eigenverb( de, az, time, dt, grazing, speed, frequencies,
                      position, ndirection, boundary_loss, verb ) ;
    switch (ID) {
        case SOURCE_ID:
            _surface.push_back( verb ) ;
            break ;
        default:
            ID -= _source_origin - 1 ;
            _upper.at(ID).push_back( verb ) ;
            break ;
    }
}

/**
 * Places an eigenverb into the class of "lower" bins to be used for the overall
 * reverberation calculation.
 */
void eigenverb_monostatic::notifyLowerCollision( unsigned de, unsigned az, double time,
               double dt, double grazing, double speed, const seq_vector& frequencies,
               const wposition1& position, const wvector1& ndirection,
               const vector<double>& boundary_loss, unsigned ID )
{
    #ifdef COLLISION_DEBUG
        std::cout << "**** Entering eigenverb_monostatic::notifyLowerCollision" << std::endl ;
        std::cout << "de: " << de << " az: " << az << " time: " << time << std::endl ;
        std::cout << "grazing: " << grazing << " ID: " << ID << std::endl ;
    #endif
    eigenverb verb ;
    create_eigenverb( de, az, time, dt, grazing, speed, frequencies,
                      position, ndirection, boundary_loss, verb ) ;
    switch (ID) {
        case SOURCE_ID:
            _bottom.push_back( verb ) ;
            break ;
        default:
            ID -= _source_origin - 1 ;
            _lower.at(ID).push_back( verb ) ;
            break ;
    }
}

/**
 * Computes the energy contributions to the reverberation
 * energy curve from the bottom interactions.
 */
void eigenverb_monostatic::compute_bottom_energy() {
    convolve_eigenverbs( _bottom, _bottom_boundary ) ;
}

/**
 * Computes the energy contributions to the reverberation
 * energy curve from the surface interactions.
 */
void eigenverb_monostatic::compute_surface_energy() {
    convolve_eigenverbs( _surface, _surface_boundary ) ;
}

/**
 * Compute all of the upper collision contributions due
 * to interactions with the volume layers
 */
void eigenverb_monostatic::compute_upper_volume_energy() {
    unsigned layer = 0 ;
    for(std::vector<std::vector<eigenverb> >::iterator k=_upper.begin();
            k!=_upper.end() && layer <= _n; ++k)
    {
        boundary_model* current_layer = _volume_boundary->getLayer(layer) ;
        convolve_eigenverbs( *k, current_layer) ;
        ++layer ;
    }
}

/**
 * Compute all of the lower collision contributions due
 * to interactions with the volume layers
 */
void eigenverb_monostatic::compute_lower_volume_energy() {
    unsigned layer = 0 ;
    for(std::vector<std::vector<eigenverb> >::iterator k=_upper.begin();
            k!=_upper.end() && layer <= _n; ++k)
    {
        boundary_model* current_layer = _volume_boundary->getLayer(layer) ;
        convolve_eigenverbs( *k, current_layer) ;
        ++layer ;
    }
}

/**
 * Convolution of the set of eigenverbs with itself from this boundary
 * and makes contributions to the reverberation levels curve when
 * a contribution is significant enough.
 */
void eigenverb_monostatic::convolve_eigenverbs( std::vector<eigenverb>& set,
                                                boundary_model* boundary )
{
    for(std::vector<eigenverb>::iterator i=set.begin();
            i!=set.end(); ++i)
    {
        eigenverb u = (*i) ;
        for(std::vector<eigenverb>::iterator j=set.begin();
                j!=set.end(); ++j)
        {
            eigenverb v = (*j) ;
            double travel_time = u.time + v.time ;
                // Don't make contributions anymore if the travel time
                // is greater then the max reverberation curve time
            if( _max_time <= travel_time ) continue ;
            compute_contribution( u, v, boundary ) ;
        }
    }
}
