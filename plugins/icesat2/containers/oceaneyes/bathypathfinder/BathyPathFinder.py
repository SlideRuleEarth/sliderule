# Copyright (c) 2024, Forrest Corcoran/Oregon State University.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import numpy as np
import pandas as pd
import networkx as nx
from sklearn.linear_model import RANSACRegressor
from sklearn.neighbors import BallTree


class BathyPathSearch:
    """
    The Sea Surface/Bathymetry Photon Classifier by RANSAC/A*
    
    Parameters:
    -----------
    
    tau (float) :: The RANSAC residual distance. This can be thought 
                   of as the sea surface thickness, or approximately
                   half the wave height. 
                  (default=0.5)
                   
    k (int)     :: The number of neighbors in the BallTree subprocess.
                   This controls the connectedness of the bathymetry.
                   Values between 10-20 are recommended. 
                   (default=15)
                   
    n (int)     :: The percentile for edge pruning. Must be between 
                   0-100. Smaller values for n will remove more 
                   edges. Values between 95-99 are recommended.
                   (default=99)
                   
    Attributes:
    -----------
    
    sea_surface_photons (pandas.DataFrame) :: Dataframe containing distances/heights of sea surface photons
    
    bathy_photons (pandas.DataFrame)       :: Dataframe containing distances/heights of bathymetric photons
    
    G (networkx.Graph)                     :: Graph of non-seasurface photons used to generate bathymetry
    
    S (List[networkx.Graph])               :: List of communication classes in G
                   
    """
    
    def __init__(self, tau=0.5, k=15, n=99):
        
        # Model parameters
        self.tau = tau
        self.k = k
        self.n = n
        
        # dataframe for passing intermediate results
        self.df = pd.DataFrame.from_dict({"along_track_dist" : [], "height" : []})
        
        # Ball Tree (G) and Communication Classes (S)
        self.G = nx.Graph()
        self.S = []
        
    def fit(self, distance, height):
        """
        Fit the model to a set of photons.
        
        Parameters:
        -----------
        
        distance (List[float]) :: The along-track-distances of the photons.
        
        height (List[float])   :: The orthometric heights of photons.
        
        """
        
        self._sea_surface(distance, height)
        self._build_graph()
        self._prune_edges()
        self._find_path()
        
        
    def _sea_surface(self, distance, height):
        """
        Fit RANSAC to the sea surface and classify inliers.
        
        Parameters:
        -----------
        
        distance (List[float]) :: The along-track-distances of the photons.
        
        height (List[float])   :: The orthometric heights of photons.
        
        """
        # Organize input photons into pandas dictionary
        self.df["along_track_dist" ], self.df["height"] =  distance, height

        # Init RANSAC with tau ~= half wave height
        ransac = RANSACRegressor(residual_threshold=self.tau)

        # Fit RANSAC to sea surface
        ransac.fit(
            self.df['along_track_dist'].values.reshape(-1,1),  
            self.df['height'].values.reshape(-1,1)
        )

        # Store and remove sea surface photons
        self.sea_surface_photons = self.df[ransac.inlier_mask_]
        self.df = self.df[~ransac.inlier_mask_]

        # Store approximate sea surface height for refraction correction
        self.df['ransac_height'] = self.df['along_track_dist'].apply(lambda y: ransac.predict(np.array(y).reshape(1,-1))[0][0])

        # Drop remaining photons above approx sea surface (i.e., land/cloud returns)
        self.df = self.df.loc[self.df['height'] < self.df['ransac_height']]
        
        
    def _build_graph(self):
        """Build the network used search for bathymetric photons"""
        
        # Initialize Tree
        Tree = BallTree(self.df[['along_track_dist', 'height']].values)

        # Build weighted graph from edges
        # Could this be vectorized? 
        # If not, maybe parallelized? 
        # This is a good opportunity for optimization
        for i, row in self.df.iterrows():

            dist, idx = Tree.query(row[['along_track_dist', 'height']].values.reshape(1,-1), k=self.k)
            dist, idx = dist.tolist()[0], idx.tolist()[0]
            
            idx = self.df.index[idx]
                        
            self.G.add_weighted_edges_from([(i,j,w) for (j,w) in zip(idx, dist)])
            
            
    def _prune_edges(self):
        """
        Remove edges from graph greater than n-th percentile in weight,
        then split the graph into communication classes.
        """
        
        # define datatypes when converting list of tuples to np array
        dt=np.dtype('int,int,object')
        
        # Convert list of edges [(node1, node2, weight), ...] to array
        E = np.array(list(self.G.edges(data=True)), dtype=dt)
        
        # Get list of edge weights
        W = [w['weight'] for w in E['f2']]
        
        # get indices of weights < n-th percentile
        idx = np.where(W >= np.percentile(W,self.n))[0]
        
        # Drop large weight edges
        E_drop = list(zip(E['f0'][idx], E['f1'][idx]))
        self.G.remove_edges_from(E_drop)

        # Get list of connected subgraphs (i.e., communication classes).
        self.S = [self.G.subgraph(c).copy() for c in nx.connected_components(self.G)]
        
    def _find_path(self):
        """
        Find targets and sources for each communication class,
        then search for shortest path between them.
        """
        
        # Empty dataframe to accumuulate selected points
        bathy_points = [pd.DataFrame.from_dict({"along_track_dist" : [], "height" : []})]
        
        # Compute shortest path for each subgraph 
        # This allows discontinuties for missing bathy
        for i, G_i in enumerate(self.S):
                       
            nodes = list(G_i.nodes)
            sdf = self.df.loc[self.df.index.isin(nodes)]
            
            # Need at least 3 photons to make a path
            if len(sdf) > 2:
                      
                # Find subgraph edges
                smin, smax = sdf['along_track_dist'].min(), sdf['along_track_dist'].max()

                # Initialize telescoping edge bins
                # Target/source = height photons in edge bins
                # If bins too large target == source, so reduce bin size
                edge_bin_width = 5

                # Get heights of all photons in edge bins
                source_h = sdf[sdf['along_track_dist'] <= smin+edge_bin_width][['height']]
                target_h = sdf[sdf['along_track_dist'] >= smax-edge_bin_width][['height']]

                # Find photons with median heights
                # Note: for even numbers of photons, median will not correspond to a photon
                # therefore, we find the "middle" photon (not technically median)
                source = source_h.sort_values(by='height').iloc[int(len(source_h)/2)].name
                target = target_h.sort_values(by='height').iloc[int(len(target_h)/2)].name

                # Find indices of photons that make up A* shortest path
                shortest_path = nx.astar_path(G_i, source, target)
    
                # Shortest path photons (excluding target, source)
                path_df = self.df.loc[self.df.index.isin(shortest_path[1:-1])].copy()
                path_df['communication class'] = i
                bathy_points.append(path_df)
            
            else:
                continue
                
        self.bathy_photons = pd.concat(bathy_points)
