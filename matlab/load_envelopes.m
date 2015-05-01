%
% Loads reverberation envelope into Matlab structure
%
% The reverberation envelope holds the data for each combination
% of azimuth, source beam, receiver beam, freqeuncy, and time.
%
%       num_src_beams   number of intensity source beams (2nd index)
%       num_rcv_beams   number of intensity receiver (3rd index)
%       num_freq        number of frequencies (4th index)
%       num_time        number of travel times (5th index)
%       travel_time     path travel time (secs).
%       frequency       frequency (Hz)
%       intensity       envelope intensity level (dB)
%
function envelopes = load_envelopes( filename )

% read netCDF data into local variables

start = 1 ;
count = inf ;
travel_time = ncread(filename,'travel_time',start,count) ;
frequency = ncread(filename,'frequency',start,count) ;

start = [1, 1, 1, 1, 1] ;
count = [inf, inf, inf, inf, inf] ;
intensity = ncread(filename,'intensity',start,count) ;
intensity = permute(intensity,[5 4 3 2 1]);

[ num_azimuths, num_src_beams , num_rcv_beams, num_freq, num_time ] ...
    = size( intensity ) ;

% translate data into structure

envelopes = struct( ...
    'num_azimuths', num_azimuths, ...
    'num_src_beams', num_src_beams, ...
    'num_rcv_beams', num_rcv_beams, ...
    'num_freq', num_freq, ...
    'num_time', num_time, ...
    'travel_time', travel_time, ...
    'frequency', frequency, ...
    'intensity', intensity ) ;

end
